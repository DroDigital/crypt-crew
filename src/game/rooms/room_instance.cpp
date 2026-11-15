#include "game/rooms/room_instance.h"

#include <cmath>
#include <raymath.h>

#include "engine/core/log.h"
#include "game/data/game_data.h"

namespace game {
namespace {

constexpr const char* kAtlas = "assets/kaykit_dungeon/textures/dungeon_texture.png";

uint32_t hash2(int a, int b, uint32_t seed) {
    uint32_t h = seed ^ (static_cast<uint32_t>(a) * 0x9E3779B1u) ^ (static_cast<uint32_t>(b) * 0x85EBCA77u);
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

uint32_t strHash(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
    return h;
}

Matrix placement(Vector3 pos, float yawRad, float scale = 1.f) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(yawRad)),
                          MatrixTranslate(pos.x, pos.y, pos.z));
}

// Half extents of a model's AABB after a yaw that is a multiple of 90 degrees.
Vector3 rotatedHalfExtents(const eng::ModelAsset* m, float yawDeg, float scale) {
    Vector3 he{(m->bounds.max.x - m->bounds.min.x) * 0.5f * scale, (m->bounds.max.y - m->bounds.min.y) * 0.5f * scale,
               (m->bounds.max.z - m->bounds.min.z) * 0.5f * scale};
    const int q = static_cast<int>(std::lround(yawDeg / 90.f)) & 3;
    if (q == 1 || q == 3) std::swap(he.x, he.z);
    return he;
}

Vector3 boundsCenterOffset(const eng::ModelAsset* m, float yawRad, float scale) {
    Vector3 c{(m->bounds.max.x + m->bounds.min.x) * 0.5f * scale, (m->bounds.max.y + m->bounds.min.y) * 0.5f * scale,
              (m->bounds.max.z + m->bounds.min.z) * 0.5f * scale};
    return Vector3RotateByAxisAngle(c, Vector3{0, 1, 0}, yawRad);
}

}  // namespace

std::string dungeonModelPath(const std::string& name) { return "assets/kaykit_dungeon/models/" + name + ".glb"; }

Vector3 RoomInstance::tileCenter(int tx, int tz) const {
    const float w = static_cast<float>(tpl->width), h = static_cast<float>(tpl->height);
    return Vector3{(static_cast<float>(tx) - w * 0.5f + 0.5f) * tile, 0.f, (static_cast<float>(tz) - h * 0.5f + 0.5f) * tile};
}

Vector3 RoomInstance::doorEntry(Side s, float depth) const {
    const DoorInstance& d = doors[static_cast<size_t>(s)];
    return Vector3Add(d.center, Vector3Scale(d.inward, depth));
}

void RoomInstance::setDoorOpen(Side s, bool open, eng::PhysicsWorld& phys) {
    DoorInstance& d = doors[static_cast<size_t>(s)];
    if (!d.socket || !d.connected || d.open == open) return;
    d.open = open;
    if (open && d.collider >= 0) {
        phys.removeStatic(d.collider);
        d.collider = -1;
    } else if (!open && d.collider < 0) {
        const bool alongX = s == kNorth || s == kSouth;
        d.collider = phys.addStaticBox(Vector3{d.center.x, 2.f, d.center.z},
                                       alongX ? Vector3{tile * 0.5f, 2.f, 0.5f} : Vector3{0.5f, 2.f, tile * 0.5f});
    }
}

void RoomInstance::updateDoors(float dt) {
    for (auto& d : doors) {
        const float target = d.open ? 1.f : 0.f;
        const float speed = 1.6f;
        if (d.openT < target) d.openT = std::min(target, d.openT + dt * speed);
        else if (d.openT > target) d.openT = std::max(target, d.openT - dt * speed * 2.f);
    }
}

void RoomInstance::draw(eng::Renderer& r) const {
    for (const RoomDraw& d : statics) r.drawModel(*d.model, d.xf);
    for (size_t i = 0; i < doors.size(); ++i) {
        const DoorInstance& d = doors[i];
        if (!d.socket || !d.connected) continue;
        if (static_cast<int>(i) == kSouth) {
            // South doors stay low so the camera can see the players: a low barrier that sinks away.
            if (lowWallModel && d.openT < 0.999f) {
                Vector3 p = d.center;
                p.y = -1.2f * d.openT;
                r.drawModel(*lowWallModel, placement(p, d.yaw), eng::DrawParams{nullptr, 0, Color{255, 200, 170, 255}, 0.f,
                                                                               Vector3{0.15f, 0.05f, 0.f}, 0});
            }
            continue;
        }
        if (doorFrameModel) r.drawModel(*doorFrameModel, placement(d.center, d.yaw));
        // The gate sinks into the floor as it opens.
        if (gateModel && d.openT < 0.999f) {
            Vector3 p = d.center;
            p.y = -4.1f * d.openT;
            r.drawModel(*gateModel, placement(p, d.yaw, 0.98f));
        }
    }
}

void buildRoom(const RoomTemplate& tpl, const FloorTheme& theme, eng::AssetCache& assets, eng::PhysicsWorld& phys,
               const std::array<bool, 4>& connected, RoomInstance& out) {
    out = RoomInstance{};
    out.tpl = &tpl;
    phys.clearStatic();
    auto model = [&](const std::string& name) { return assets.model(dungeonModelPath(name), kAtlas); };

    // Tile size comes from the floor mesh AABB (docs/asset_report.md: floor_tile_large is 4 x 4).
    const eng::ModelAsset* baseFloor = model("floor_tile_large");
    if (baseFloor && baseFloor->model.meshCount > 0) out.tile = baseFloor->bounds.max.x - baseFloor->bounds.min.x;
    const float T = out.tile;
    const float half = T * 0.5f;
    const uint32_t seed = strHash(tpl.id);

    out.wallModel = model("wall");
    out.gateModel = model("wall_gated");
    out.doorFrameModel = model("wall_doorway_sides");
    out.lowWallModel = model("barrier");
    const eng::ModelAsset* wallCracked = model("wall_cracked");
    const eng::ModelAsset* wallBroken = model("wall_broken");
    const eng::ModelAsset* lowWall = model("barrier");
    const eng::ModelAsset* torch = model("torch_mounted");
    const eng::ModelAsset* banner = theme.banner.empty() ? nullptr : model(theme.banner);
    const eng::ModelAsset* cornerPiece = model("wall_corner_small");

    const float W = static_cast<float>(tpl.width) * T, H = static_cast<float>(tpl.height) * T;
    out.boundsMin = Vector3{-W * 0.5f, 0, -H * 0.5f};
    out.boundsMax = Vector3{W * 0.5f, 0, H * 0.5f};

    // Nav grid: 1 m cells over the room's bounds, everything blocked until floor is marked.
    const float cell = 1.f;
    const int nw = static_cast<int>(std::ceil(W / cell)), nh = static_cast<int>(std::ceil(H / cell));
    out.nav.init(nw, nh, cell, out.boundsMin);
    for (int z = 0; z < nh; ++z)
        for (int x = 0; x < nw; ++x) out.nav.setBlocked(x, z, true);

    for (int s = 0; s < 4; ++s) {
        DoorInstance& d = out.doors[static_cast<size_t>(s)];
        d.socket = tpl.hasDoor(static_cast<Side>(s));
        d.connected = d.socket && connected[static_cast<size_t>(s)];
    }

    // Pass 1: every floor tile's nav cells are walkable. Blockers (walls, props) come after, so a
    // large prop overhanging into a neighbor tile stays blocked.
    for (int tz = 0; tz < tpl.height; ++tz)
        for (int tx = 0; tx < tpl.width; ++tx) {
            if (!tpl.isFloor(tx, tz)) continue;
            const Vector3 c = out.tileCenter(tx, tz);
            for (int z = 0; z < nh; ++z)
                for (int x = 0; x < nw; ++x) {
                    const Vector3 p = out.nav.cellCenter(x, z);
                    if (std::fabs(p.x - c.x) < half && std::fabs(p.z - c.z) < half) out.nav.setBlocked(x, z, false);
                }
        }
    for (int tz = 0; tz < tpl.height; ++tz) {
        for (int tx = 0; tx < tpl.width; ++tx) {
            const LegendEntry* e = tpl.at(tx, tz);
            if (!e || e->isVoid) continue;
            const Vector3 c = out.tileCenter(tx, tz);
            // Floor mesh + collider.
            if (const eng::ModelAsset* fm = model(e->floor))
                out.statics.push_back(RoomDraw{fm, MatrixTranslate(c.x, 0, c.z), c});
            phys.addStaticBox(Vector3{c.x, -0.25f, c.z}, Vector3{half, 0.25f, half});
            // Tile prop.
            if (!e->prop.empty()) {
                if (const eng::ModelAsset* pm = model(e->prop)) {
                    const float yaw = e->rot * DEG2RAD;
                    out.statics.push_back(RoomDraw{pm, placement(c, yaw), c});
                    if (e->collide) {
                        const Vector3 he = rotatedHalfExtents(pm, e->rot, 1.f);
                        const Vector3 off = boundsCenterOffset(pm, yaw, 1.f);
                        const Vector3 bc{c.x + off.x, he.y, c.z + off.z};
                        phys.addStaticBox(bc, he);
                        out.nav.blockBox(bc, he, kNavInflate);
                    }
                }
            }
            // Boundary edges: walls, low walls (south, so the camera sees players), doors.
            static const int kDx[4] = {0, 1, 0, -1};
            static const int kDz[4] = {-1, 0, 1, 0};
            for (int s = 0; s < 4; ++s) {
                if (tpl.isFloor(tx + kDx[s], tz + kDz[s])) continue;
                const Side side = static_cast<Side>(s);
                const Vector3 ec{c.x + static_cast<float>(kDx[s]) * half, 0.f, c.z + static_cast<float>(kDz[s]) * half};
                const Vector3 inward{-static_cast<float>(kDx[s]), 0.f, -static_cast<float>(kDz[s])};
                const bool alongX = side == kNorth || side == kSouth;
                const float yaw = alongX ? 0.f : PI * 0.5f;
                int dtx = -1, dtz = -1;
                if (tpl.hasDoor(side)) tpl.doorTile(side, dtx, dtz);
                const bool isDoor = tpl.hasDoor(side) && dtx == tx && dtz == tz;
                const Vector3 colHalf = alongX ? Vector3{half, 2.f, 0.5f} : Vector3{0.5f, 2.f, half};
                // Block nav cells along the edge (wall thickness + agent radius); doorways stay open.
                if (!(isDoor && out.doors[static_cast<size_t>(s)].connected))
                    out.nav.blockBox(ec, Vector3{alongX ? half : 0.5f, 0, alongX ? 0.5f : half}, kNavInflate);
                if (isDoor && out.doors[static_cast<size_t>(s)].connected) {
                    DoorInstance& d = out.doors[static_cast<size_t>(s)];
                    d.center = ec;
                    d.inward = inward;
                    d.yaw = yaw;
                    d.collider = phys.addStaticBox(Vector3{ec.x, 2.f, ec.z}, colHalf);
                    continue;
                }
                phys.addStaticBox(Vector3{ec.x, 2.f, ec.z}, colHalf);
                if (isDoor) {
                    DoorInstance& d = out.doors[static_cast<size_t>(s)];
                    d.center = ec;
                    d.inward = inward;
                    d.yaw = yaw;
                }
                if (side == kSouth) {
                    if (lowWall) out.statics.push_back(RoomDraw{lowWall, placement(ec, 0.f), ec});
                    continue;
                }
                const uint32_t h = hash2(tx, tz * 4 + s, seed);
                const eng::ModelAsset* wm = out.wallModel;
                if (h % 9 == 0 && wallCracked) wm = wallCracked;
                else if (h % 13 == 1 && wallBroken) wm = wallBroken;
                out.statics.push_back(RoomDraw{wm, placement(ec, yaw), ec});
                // Torches on every other wall tile; banners in between (north wall only).
                const bool torchHere = ((side == kNorth ? tx : tz) % 2) == 1;
                const Vector3 face = Vector3Add(ec, Vector3Scale(inward, 0.5f));
                const float faceYaw = std::atan2(inward.x, inward.z);
                if (torchHere && torch) {
                    out.statics.push_back(RoomDraw{torch, placement(Vector3{face.x, 2.4f, face.z}, faceYaw), face});
                    const Vector3 lp = Vector3Add(Vector3{face.x, 3.0f, face.z}, Vector3Scale(inward, 0.9f));
                    out.lights.push_back(eng::PointLight{lp, theme.torchColor, theme.torchRadius});
                } else if (!torchHere && side == kNorth && banner) {
                    out.statics.push_back(RoomDraw{banner, placement(Vector3{face.x, 0.f, face.z}, faceYaw), face});
                }
            }
        }
    }
    // Small corner caps where two full walls meet at an outer corner (N/E/W sides).
    if (cornerPiece) {
        for (int tz = 0; tz < tpl.height; ++tz)
            for (int tx = 0; tx < tpl.width; ++tx) {
                if (!tpl.isFloor(tx, tz)) continue;
                const Vector3 c = out.tileCenter(tx, tz);
                const bool n = !tpl.isFloor(tx, tz - 1), w = !tpl.isFloor(tx - 1, tz), e = !tpl.isFloor(tx + 1, tz);
                if (n && w) out.statics.push_back(RoomDraw{cornerPiece, placement(Vector3{c.x - half, 0, c.z - half}, 0.f), c});
                if (n && e) out.statics.push_back(RoomDraw{cornerPiece, placement(Vector3{c.x + half, 0, c.z - half}, -PI * 0.5f), c});
            }
    }
    // Explicit prop placements.
    for (const PropPlacement& p : tpl.props) {
        const eng::ModelAsset* pm = model(p.model);
        if (!pm) continue;
        const Vector3 c = Vector3Add(out.tileCenter(p.tx, p.tz), p.offset);
        const float yaw = p.rot * DEG2RAD;
        out.statics.push_back(RoomDraw{pm, placement(c, yaw, p.scale), c});
        if (p.collide) {
            const Vector3 he = rotatedHalfExtents(pm, p.rot, p.scale);
            const Vector3 off = boundsCenterOffset(pm, yaw, p.scale);
            const Vector3 bc{c.x + off.x, he.y, c.z + off.z};
            phys.addStaticBox(bc, he);
            out.nav.blockBox(bc, he, kNavInflate);
        }
    }
    // A soft fill light over the room center keeps players readable everywhere.
    out.lights.push_back(eng::PointLight{Vector3{0, 9.f, 2.f}, theme.fillColor, std::max(W, H) * 0.75f});
    phys.finalizeStatic();
}

}  // namespace game
