#pragma once
// Room templates (data/rooms/*.json): an ASCII tile grid + legend + props + door sockets +
// spawn markers. Meshes, static colliders and the nav grid are all built from this.
#include <raylib.h>

#include <array>
#include <map>
#include <string>
#include <vector>

#include "engine/core/json_util.h"

namespace game {

enum class RoomType { Combat, Treasure, Trap, Shrine, Boss, Start };
const char* roomTypeName(RoomType t);

struct LegendEntry {
    std::string floor;    // dungeon model name (empty for void)
    std::string prop;     // optional prop model on this tile
    float rot = 0.f;      // prop rotation (degrees)
    bool isVoid = false;
    bool collide = true;  // prop blocks movement
    std::string spawn;    // enemy id
    std::string marker;   // chest | statue | merchant | trap | start
};

struct PropPlacement {
    std::string model;
    int tx = 0, tz = 0;
    Vector3 offset{0, 0, 0};
    float rot = 0.f;
    float scale = 1.f;
    bool collide = true;
};

struct TileMarker {
    int tx = 0, tz = 0;
    std::string kind;  // enemy id for spawns, marker name otherwise
};

enum Side : int { kNorth = 0, kEast = 1, kSouth = 2, kWest = 3 };
Side oppositeSide(Side s);
const char* sideName(Side s);

struct RoomTemplate {
    std::string id, file;
    RoomType type = RoomType::Combat;
    int width = 0, height = 0;  // tiles (x, z)
    std::vector<std::string> grid;
    std::map<char, LegendEntry> legend;
    std::array<int, 4> doors{-1, -1, -1, -1};  // tile index along each side, -1 = no socket
    std::vector<PropPlacement> props;
    std::vector<TileMarker> spawns;
    std::vector<TileMarker> markers;
    int waves = 1;

    const LegendEntry* at(int tx, int tz) const;
    bool isFloor(int tx, int tz) const;
    bool hasDoor(Side s) const { return doors[static_cast<size_t>(s)] >= 0; }
    // Floor tile next to the given door socket.
    void doorTile(Side s, int& tx, int& tz) const;
};

// Parses one template; `defaults` is the shared legend (data/rooms/_legend.json).
RoomTemplate parseRoomTemplate(const eng::json& j, const std::map<char, LegendEntry>& defaults, const std::string& ctx);
std::map<char, LegendEntry> parseLegend(const eng::json& j, const std::string& ctx);

}  // namespace game
