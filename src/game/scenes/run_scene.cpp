#include "game/scenes/run_scene.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>

#include "engine/core/log.h"
#include "game/data/relic_defs.h"
#include "game/enemies/enemy_system.h"
#include "game/player/dummy_ai.h"
#include "game/player/player_system.h"
#include "game/ui/glyphs.h"
#include "game/world/combat.h"
#include "game/world/relic_hooks.h"

namespace game {
namespace {

bool finite3(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

}  // namespace

RunScene::RunScene(App& app, const RunConfig& cfg) : app_(app), cfg_(cfg) {
    WorldServices svc{app.data(), app.state(), app.assets(), app.characters(), app.physics(), app.particles(),
                      app.audio(), app.clock(), app.fx()};
    world_ = std::make_unique<World>(svc);
    world_->rng.reseed(cfg.seed);
    app.state().startRun(cfg.seed);
    for (const PlayerSetup& ps : cfg.players) {
        auto& rec = app.state().players[static_cast<size_t>(ps.slot)];
        rec.active = true;
        rec.classId = ps.classId;
        world_->addPlayer(ps.slot, ps.classId, Vector3{static_cast<float>(ps.slot) * 1.5f, 0, 0}, ps.dummy || cfg.allBots);
    }
    app.particles().clear();
    startFloor(0);
    app.audio().playMusic("explore", 1.0f);
}

RunScene::~RunScene() { world_.reset(); }

float RunScene::aspect() const {
    return static_cast<float>(std::max(GetScreenWidth(), 1)) / static_cast<float>(std::max(GetScreenHeight(), 1));
}

void RunScene::startFloor(int floor) {
    GameData& d = app_.data();
    app_.state().startFloor(floor);
    world_->theme = &d.floors[static_cast<size_t>(std::clamp(floor, 0, static_cast<int>(d.floors.size()) - 1))];
    plan_ = FloorPlan{};
    // Single-room mode: one room whose doors all lead back to itself.
    RoomNode n;
    n.templateId = cfg_.singleRoom.empty() ? d.rooms.begin()->first : cfg_.singleRoom;
    n.type = d.room(n.templateId).type;
    n.neighbor = {0, 0, 0, 0};
    plan_.rooms.push_back(n);
    plan_.start = 0;
    enterRoom(0, -1);
}

void RunScene::enterRoom(int index, int fromSide) {
    World& w = *world_;
    current_ = index;
    RoomNode& node = plan_.rooms[static_cast<size_t>(index)];
    node.visited = true;
    const RoomTemplate& tpl = app_.data().room(node.templateId);
    w.clearEnemies();
    w.gold.clear();
    w.texts.clear();
    std::array<bool, 4> connected{};
    for (int s = 0; s < 4; ++s) connected[static_cast<size_t>(s)] = node.neighbor[static_cast<size_t>(s)] >= 0;
    buildRoom(tpl, *w.theme, app_.assets(), app_.physics(), connected, w.room);
    // Players arrive through the door they walked into (opposite side of the previous room).
    Vector3 base{0, 0, 0};
    Vector3 inward{0, 0, -1};
    if (fromSide >= 0 && w.room.doors[static_cast<size_t>(fromSide)].socket) {
        base = w.room.doorEntry(static_cast<Side>(fromSide), 2.5f);
        inward = w.room.doors[static_cast<size_t>(fromSide)].inward;
    } else {
        for (const TileMarker& m : tpl.markers)
            if (m.kind == "start") base = w.room.tileCenter(m.tx, m.tz);
        if (tpl.hasDoor(kSouth)) {
            base = w.room.doorEntry(kSouth, 3.f);
            inward = Vector3{0, 0, -1};
        }
    }
    const Vector3 side{inward.z, 0, -inward.x};
    for (size_t i = 0; i < w.players.size(); ++i) {
        Player& p = w.players[i];
        const float off = (static_cast<float>(i) - (static_cast<float>(w.players.size()) - 1.f) * 0.5f) * 1.6f;
        Vector3 pos = Vector3Add(base, Vector3Scale(side, off));
        pos = w.room.nav.walkable(pos) ? pos : w.room.nav.nearestWalkable(pos);
        w.placePlayer(p, pos);
        p.k.yaw = p.k.prevYaw = std::atan2(inward.x, inward.z);
        resetPlayerForRoom(p);
    }
    std::vector<Vector3> pts;
    for (const Player& p : w.players) pts.push_back(p.k.pos);
    w.camera.setBounds(w.room.boundsMin, w.room.boundsMax);
    w.camera.snap(pts, aspect());
    phase_ = Phase::Enter;
    phaseTime_ = 0.f;
    spawned_ = false;
    fade_ = 1.f;
    app_.state().roomIndex = index;
    // Rooms without enemies count as cleared at once (doors open).
    if (node.cleared || tpl.spawns.empty()) {
        for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), true, app_.physics());
        for (auto& d : w.room.doors) d.openT = d.open ? 1.f : 0.f;
        spawned_ = true;
    }
}

void RunScene::spawnRoomEnemies() {
    World& w = *world_;
    const RoomTemplate& tpl = app_.data().room(plan_.rooms[static_cast<size_t>(current_)].templateId);
    const int players = std::max(1, w.playerCount());
    for (const TileMarker& m : tpl.spawns) {
        const EnemyDef& def = app_.data().enemy(m.kind);
        const int base = w.rng.rangeInclusive(def.group[0], def.group[1]);
        const int count = def.behavior == "boss" ? 1 : app_.data().scaling.spawnCount(base, players);
        const Vector3 c = w.room.tileCenter(m.tx, m.tz);
        for (int i = 0; i < count; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(count) * 2.f * PI + w.rng.uniform(0.f, 0.5f);
            const float r = count > 1 ? 1.2f + 0.35f * static_cast<float>(i % 3) : 0.f;
            Vector3 pos{c.x + std::cos(a) * r, 0.f, c.z + std::sin(a) * r};
            if (!w.room.walkable(pos)) pos = w.room.nav.nearestWalkable(pos);
            w.spawnEnemy(m.kind, pos);
        }
    }
    spawned_ = true;
    for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), false, app_.physics());
}

void RunScene::buildCommands(float dt) {
    World& w = *world_;
    eng::InputRouter& in = app_.input();
    for (Player& p : w.players) {
        PlayerCommand c;
        if (p.dummy) {
            c = dummyCommand(w, p, dt);
        } else if (in.slotActive(p.slot)) {
            const eng::SlotInput& si = in.input(p.slot);
            c.move = si.move;
            c.held = si.held;
            c.pressed = in.consumePressed(p.slot);
            if (si.usesMouse) {
                // Mouse aim: project the cursor onto the ground plane at hip height.
                const Ray ray = app_.renderer().screenRay(si.mouse);
                if (std::fabs(ray.direction.y) > 1e-4f) {
                    const float t = (1.0f - ray.position.y) / ray.direction.y;
                    const Vector3 hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
                    const Vector3 d{hit.x - p.k.pos.x, 0, hit.z - p.k.pos.z};
                    if (Vector3Length(d) > 0.3f) {
                        c.aim = Vector3Normalize(d);
                        c.aimExplicit = true;
                    }
                }
            } else if (Vector2Length(si.aimStick) > 0.3f) {
                c.aim = Vector3Normalize(Vector3{si.aimStick.x, 0, si.aimStick.y});
                c.aimExplicit = true;
            }
        }
        cmds_[static_cast<size_t>(std::clamp(p.slot, 0, 3))] = c;
    }
}

void RunScene::onRoomCleared() {
    World& w = *world_;
    RoomNode& node = plan_.rooms[static_cast<size_t>(current_)];
    node.cleared = true;
    ++app_.state().roomsCleared;
    phase_ = Phase::Cleared;
    phaseTime_ = 0.f;
    for (int s = 0; s < 4; ++s) w.room.setDoorOpen(static_cast<Side>(s), true, app_.physics());
    w.sound("door");
    banner_ = "ROOM CLEARED";
    bannerTime_ = 2.0f;
    hooks::onRoomClear(w);
    for (Player& p : w.players)
        if (p.canAct() && p.state == PState::Move) {
            p.state = PState::Cheer;
            p.stateTime = 0.f;
            p.vis.play("cheer", true);
        }
}

void RunScene::checkDoors() {
    World& w = *world_;
    const RoomNode& node = plan_.rooms[static_cast<size_t>(current_)];
    const float depth = w.cfg("room", "door_trigger_depth");
    for (int s = 0; s < 4; ++s) {
        const DoorInstance& d = w.room.doors[static_cast<size_t>(s)];
        if (!d.connected || !d.open || d.openT < 0.9f) continue;
        for (const Player& p : w.players) {
            if (!p.canAct()) continue;
            const Vector3 rel = Vector3Subtract(p.k.pos, d.center);
            const float along = -Vector3DotProduct(rel, d.inward);  // > 0 when past the door line
            const float lateral = std::fabs(Vector3DotProduct(rel, Vector3{d.inward.z, 0, -d.inward.x}));
            if (along > -depth && lateral < w.room.tile * 0.4f) {
                pendingRoom_ = node.neighbor[static_cast<size_t>(s)];
                pendingSide_ = static_cast<int>(oppositeSide(static_cast<Side>(s)));
                phase_ = Phase::Exit;
                phaseTime_ = 0.f;
                return;
            }
        }
    }
}

void RunScene::fixedUpdate(float dt) {
    World& w = *world_;
    w.time += dt;
    phaseTime_ += dt;
    app_.state().runTime += dt;
    bannerTime_ = std::max(0.f, bannerTime_ - dt);
    buildCommands(dt);
    updatePlayers(w, cmds_, dt, aspect());
    updateRevives(w, cmds_, dt);
    updateEnemies(w, dt);
    updateHits(w);
    updateProjectiles(w, dt);
    updateHazards(w, dt);
    updateGold(w, dt);
    updateTexts(w, dt);
    w.removeDeadEnemies();
    w.room.updateDoors(dt);
    app_.particles().update(dt);

    std::vector<Vector3> pts;
    for (const Player& p : w.players)
        if (p.state != PState::Dead) pts.push_back(p.k.pos);
    w.camera.update(pts, aspect(), dt);

    const float transition = w.cfg("room", "transition_time");
    switch (phase_) {
        case Phase::Enter:
            fade_ = std::max(0.f, 1.f - phaseTime_ / transition);
            if (!spawned_ && phaseTime_ > 0.5f) spawnRoomEnemies();
            if (spawned_ && phaseTime_ > transition) phase_ = Phase::Fight;
            break;
        case Phase::Fight:
            if (spawned_ && w.livingEnemies() == 0 && !plan_.rooms[static_cast<size_t>(current_)].cleared &&
                phaseTime_ > w.cfg("room", "clear_delay"))
                onRoomCleared();
            else if (plan_.rooms[static_cast<size_t>(current_)].cleared)
                checkDoors();
            break;
        case Phase::Cleared:
            if (phaseTime_ > 0.5f) checkDoors();
            break;
        case Phase::Exit:
            fade_ = std::min(1.f, phaseTime_ / transition);
            if (phaseTime_ >= transition && pendingRoom_ >= 0) {
                // Single-room mode re-arms the room so it can be fought again.
                if (!cfg_.singleRoom.empty()) plan_.rooms[static_cast<size_t>(pendingRoom_)].cleared = false;
                enterRoom(pendingRoom_, pendingSide_);
                pendingRoom_ = -1;
            }
            break;
    }
    // Everyone down: restart the room (the run summary arrives in M2/M4).
    if (w.allPlayersDown()) {
        gameOverTime_ += dt;
        if (gameOverTime_ > 3.f) {
            for (Player& p : w.players) {
                p.hp.hp = p.hp.max;
                p.state = PState::Move;
            }
            plan_.rooms[static_cast<size_t>(current_)].cleared = false;
            enterRoom(current_, -1);
            gameOverTime_ = 0.f;
        }
    }
    if (app_.state().purse != lastPurse_) {
        goldPulse_ = 1.f;
        lastPurse_ = app_.state().purse;
    }
    goldPulse_ = std::max(0.f, goldPulse_ - dt * 3.f);
}

void RunScene::frameUpdate(float) {}

bool RunScene::healthy(std::string& why) const {
    for (const Player& p : world_->players)
        if (!finite3(p.k.pos) || !std::isfinite(p.hp.hp)) {
            why = "player " + std::to_string(p.slot) + " has a NaN position/hp";
            return false;
        }
    for (const Enemy& e : world_->enemies)
        if (!finite3(e.k.pos) || !std::isfinite(e.hp.hp)) {
            why = "enemy " + e.def->id + " has a NaN position/hp";
            return false;
        }
    for (const Projectile& pr : world_->projectiles)
        if (!finite3(pr.pos)) {
            why = "projectile NaN";
            return false;
        }
    return true;
}

void RunScene::renderWorld(float alpha) {
    World& w = *world_;
    eng::Renderer& r = app_.renderer();
    const Camera3D cam = w.camera.camera(w.time);
    std::vector<eng::PointLight> lights = w.room.lights;
    for (const Projectile& pr : w.projectiles) {
        if (lights.size() > 40) break;
        lights.push_back(eng::PointLight{pr.pos, pr.fromPlayer ? Vector3{1.2f, 0.7f, 2.2f} : Vector3{0.6f, 2.f, 0.5f}, 5.f});
    }
    for (const Hazard& h : w.hazards)
        if (h.kind != Hazard::Kind::ShockRing && lights.size() < 48)
            lights.push_back(eng::PointLight{Vector3Add(h.pos, Vector3{0, 1, 0}), Vector3{1.6f, 0.7f, 0.2f}, 4.f});
    r.beginScene(cam, w.theme->lighting, lights);
    w.room.draw(r);

    for (Player& p : w.players) {
        const Vector3 pos = p.k.lerpPos(alpha);
        Color tint = WHITE;
        Vector3 em{0, 0, 0};
        if (p.rage > 0.f) em = Vector3{0.35f, 0.05f, 0.f};
        if (p.guard > 0.f) em = Vector3{0.05f, 0.12f, 0.3f};
        if (p.state == PState::Downed) tint = Color{170, 150, 150, 255};
        if (p.state == PState::Dead) tint = Color{110, 100, 100, 255};
        if (p.state == PState::Dodge || p.hp.iframes > 0.f) {
            // Brief blink while invulnerable after a hit.
            if (p.state != PState::Dodge && std::fmod(w.time, 0.16f) < 0.05f) tint.a = 255, em = Vector3Add(em, Vector3{0.15f, 0.15f, 0.15f});
        }
        p.vis.draw(r, pos, p.k.lerpYaw(alpha), p.hp.flash > 0.f ? 1.f : 0.f, tint, em);
        r.shadow(pos, 0.9f, 0.5f);
        // Player ring in slot color; revive progress ring under downed players.
        Color sc = slotColor(p.slot);
        sc.a = 150;
        r.decal(pos, 1.0f, sc, eng::DecalShape::Ring);
        if (p.state == PState::Downed) {
            const float k = 0.5f + 0.5f * std::sin(w.time * 6.f);
            r.decal(pos, w.cfg("revive", "radius"), Color{255, 80, 70, static_cast<unsigned char>(80 + 60 * k)}, eng::DecalShape::Ring);
        }
    }
    for (Enemy& e : w.enemies) {
        Vector3 pos = e.k.lerpPos(alpha);
        if (e.state == EState::Dead && e.deadTime > 1.8f) pos.y -= (e.deadTime - 1.8f) * 1.2f;  // sink away
        const float glow = telegraphGlow(e);
        const Vector3 em = e.def->behavior == "caster" ? Vector3{0.2f * glow, 0.9f * glow, 0.2f * glow}
                                                       : Vector3{1.0f * glow, 0.1f * glow, 0.05f * glow};
        e.vis.draw(r, pos, e.k.lerpYaw(alpha), e.hp.flash > 0.f ? 1.f : 0.f, e.def->tint, em);
        if (e.state != EState::Dead) r.shadow(pos, e.k.radius * 1.7f, 0.5f);
        // Telegraph decals: lunge lane, wind-up warning.
        if (e.state == EState::Telegraph) {
            for (int i = 1; i <= 6; ++i)
                r.decal(Vector3Add(pos, Vector3Scale(e.lungeDir, static_cast<float>(i) * 1.2f)), 0.7f,
                        Color{255, 40, 30, static_cast<unsigned char>(110 + 80 * glow)}, eng::DecalShape::Disc);
        }
    }
    const eng::ModelAsset* coin = app_.assets().find("assets/kaykit_dungeon/models/coin.glb");
    for (const GoldPickup& g : w.gold) {
        if (coin) r.drawModel(*coin, MatrixMultiply(MatrixMultiply(MatrixScale(1.6f, 1.6f, 1.6f), MatrixRotateY(g.age * 6.f)),
                                                     MatrixTranslate(g.pos.x, g.pos.y, g.pos.z)),
                              eng::DrawParams{nullptr, 0, WHITE, 0.f, Vector3{0.35f, 0.25f, 0.f}, 0});
        r.shadow(Vector3{g.pos.x, 0, g.pos.z}, 0.3f, 0.4f);
    }
    for (const Hazard& h : w.hazards) {
        if (h.kind == Hazard::Kind::ShockRing) {
            r.decal(h.pos, h.radius, Color{255, 120, 60, 220}, eng::DecalShape::Ring);
        } else if (h.warmup > 0.f) {
            r.decal(h.pos, h.radius, Color{255, 40, 30, 160}, eng::DecalShape::Disc);
        } else {
            r.decal(h.pos, h.radius, Color{255, 120, 30, 90}, eng::DecalShape::Disc);
        }
    }
    for (const Projectile& pr : w.projectiles)
        if (pr.kind == ProjKind::Bomb)
            r.decal(Vector3{pr.pos.x, 0, pr.pos.z}, pr.radius, Color{255, 60, 30, 140}, eng::DecalShape::Ring);
    r.flushDecals();
    app_.particles().draw(r);
    r.endScene();
}

void RunScene::render(float alpha) {
    World& w = *world_;
    renderWorld(alpha);
    app_.renderer().present(fade_);
    const eng::Ui& ui = app_.ui();
    drawFloatingTexts(ui, app_.renderer(), w);
    HudInfo info;
    info.floor = app_.state().floor;
    info.room = current_ + 1;
    info.rooms = static_cast<int>(plan_.rooms.size());
    info.floorName = w.theme->name.c_str();
    info.roomType = roomTypeName(plan_.rooms[static_cast<size_t>(current_)].type);
    info.goldPulse = goldPulse_;
    info.banner = bannerTime_ > 0.f ? banner_.c_str() : nullptr;
    info.bannerAlpha = std::min(1.f, bannerTime_);
    drawHud(ui, app_.renderer(), w, app_.input(), info);
}

}  // namespace game
