#pragma once
// Seeded floor generator: a chain of rooms on a grid plus one side branch (treasure).
#include <cstdint>

#include "game/data/game_data.h"
#include "game/rooms/floor_plan.h"

namespace game {

// floor: 0-based. The last floor ends in the boss room, the others in a shrine.
FloorPlan generateFloor(const GameData& data, int floor, uint64_t seed);
// Checks the invariants (connected, 6-8 rooms, one branch, symmetric doors). Returns "" if valid.
std::string validatePlan(const FloorPlan& plan, int minRooms, int maxRooms);

}  // namespace game
