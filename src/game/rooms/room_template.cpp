#include "game/rooms/room_template.h"

#include <stdexcept>

namespace game {

const char* roomTypeName(RoomType t) {
    switch (t) {
        case RoomType::Combat: return "combat";
        case RoomType::Treasure: return "treasure";
        case RoomType::Trap: return "trap";
        case RoomType::Shrine: return "shrine";
        case RoomType::Boss: return "boss";
        case RoomType::Start: return "start";
    }
    return "?";
}

Side oppositeSide(Side s) { return static_cast<Side>((static_cast<int>(s) + 2) % 4); }
const char* sideName(Side s) {
    static const char* n[4] = {"N", "E", "S", "W"};
    return n[static_cast<int>(s)];
}

const LegendEntry* RoomTemplate::at(int tx, int tz) const {
    if (tx < 0 || tz < 0 || tx >= width || tz >= height) return nullptr;
    const char c = grid[static_cast<size_t>(tz)][static_cast<size_t>(tx)];
    auto it = legend.find(c);
    return it == legend.end() ? nullptr : &it->second;
}

bool RoomTemplate::isFloor(int tx, int tz) const {
    const LegendEntry* e = at(tx, tz);
    return e && !e->isVoid;
}

void RoomTemplate::doorTile(Side s, int& tx, int& tz) const {
    const int i = doors[static_cast<size_t>(s)];
    switch (s) {
        case kNorth: tx = i; tz = 0; break;
        case kSouth: tx = i; tz = height - 1; break;
        case kWest: tx = 0; tz = i; break;
        case kEast: tx = width - 1; tz = i; break;
    }
}

std::map<char, LegendEntry> parseLegend(const eng::json& j, const std::string& ctx) {
    std::map<char, LegendEntry> out;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key().size() != 1) throw std::runtime_error(ctx + ": legend keys must be one character: '" + it.key() + "'");
        LegendEntry e;
        const auto& v = *it;
        e.isVoid = eng::opt(v, "void", false);
        e.floor = eng::opt<std::string>(v, "floor", "");
        if (!e.isVoid && e.floor.empty()) throw std::runtime_error(ctx + ": legend '" + it.key() + "' needs 'floor' or 'void'");
        e.prop = eng::opt<std::string>(v, "prop", "");
        e.rot = eng::opt(v, "rot", 0.f);
        e.collide = eng::opt(v, "collide", true);
        e.spawn = eng::opt<std::string>(v, "spawn", "");
        e.marker = eng::opt<std::string>(v, "marker", "");
        out[it.key()[0]] = e;
    }
    return out;
}

RoomTemplate parseRoomTemplate(const eng::json& j, const std::map<char, LegendEntry>& defaults, const std::string& ctx) {
    RoomTemplate r;
    r.file = ctx;
    r.id = eng::req<std::string>(j, "id", ctx);
    const std::string type = eng::req<std::string>(j, "type", ctx);
    if (type == "combat") r.type = RoomType::Combat;
    else if (type == "treasure") r.type = RoomType::Treasure;
    else if (type == "trap") r.type = RoomType::Trap;
    else if (type == "shrine") r.type = RoomType::Shrine;
    else if (type == "boss") r.type = RoomType::Boss;
    else if (type == "start") r.type = RoomType::Start;
    else throw std::runtime_error(ctx + ": unknown room type '" + type + "'");
    r.grid = eng::req<std::vector<std::string>>(j, "grid", ctx);
    r.height = static_cast<int>(r.grid.size());
    if (r.height < 3) throw std::runtime_error(ctx + ": grid too small");
    r.width = static_cast<int>(r.grid[0].size());
    for (const auto& row : r.grid)
        if (static_cast<int>(row.size()) != r.width) throw std::runtime_error(ctx + ": grid rows must have equal length");
    r.legend = defaults;
    if (j.contains("legend"))
        for (auto& [k, v] : parseLegend(j.at("legend"), ctx)) r.legend[k] = v;
    r.waves = eng::opt(j, "waves", 1);

    const auto& doors = j.at("doors");
    const char* names[4] = {"N", "E", "S", "W"};
    for (int s = 0; s < 4; ++s) r.doors[static_cast<size_t>(s)] = eng::opt(doors, names[s], -1);

    for (int z = 0; z < r.height; ++z)
        for (int x = 0; x < r.width; ++x) {
            const char c = r.grid[static_cast<size_t>(z)][static_cast<size_t>(x)];
            auto it = r.legend.find(c);
            if (it == r.legend.end())
                throw std::runtime_error(ctx + ": unknown tile '" + std::string(1, c) + "' at " + std::to_string(x) + "," +
                                         std::to_string(z));
            if (!it->second.spawn.empty()) r.spawns.push_back(TileMarker{x, z, it->second.spawn});
            if (!it->second.marker.empty()) r.markers.push_back(TileMarker{x, z, it->second.marker});
        }
    // Door sockets must sit on floor tiles at the border.
    for (int s = 0; s < 4; ++s) {
        if (r.doors[static_cast<size_t>(s)] < 0) continue;
        int tx, tz;
        r.doorTile(static_cast<Side>(s), tx, tz);
        if (!r.isFloor(tx, tz))
            throw std::runtime_error(ctx + ": door " + names[s] + " is not on a border floor tile");
    }
    for (const auto& p : eng::opt(j, "props", eng::json::array())) {
        PropPlacement pp;
        pp.model = eng::req<std::string>(p, "model", ctx + ":props");
        const auto tile = eng::req<std::array<int, 2>>(p, "tile", ctx + ":props");
        pp.tx = tile[0];
        pp.tz = tile[1];
        const auto off = eng::opt(p, "offset", std::array<float, 3>{0, 0, 0});
        pp.offset = Vector3{off[0], off[1], off[2]};
        pp.rot = eng::opt(p, "rot", 0.f);
        pp.scale = eng::opt(p, "scale", 1.f);
        pp.collide = eng::opt(p, "collide", true);
        r.props.push_back(pp);
    }
    return r;
}

}  // namespace game
