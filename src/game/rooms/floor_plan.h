#pragma once
// A floor: a chain of rooms placed on a grid, with one optional side branch.
#include <array>
#include <string>
#include <vector>

#include "game/rooms/room_template.h"

namespace game {

struct RoomNode {
    std::string templateId;
    RoomType type = RoomType::Combat;
    int gx = 0, gz = 0;                         // grid cell
    std::array<int, 4> neighbor{-1, -1, -1, -1};  // room index per side (N, E, S, W)
    bool cleared = false;
    bool visited = false;
    bool branch = false;
};

struct FloorPlan {
    std::vector<RoomNode> rooms;
    int start = 0;
    int exit = -1;  // the last room (shrine or boss); walking out of it ends the floor
    int mainLength() const;
};

}  // namespace game
