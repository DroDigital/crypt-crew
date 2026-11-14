#pragma once
#include <array>

#include "game/world/world.h"

namespace game {

// Advances all players one fixed step from their commands.
void updatePlayers(World& w, const std::array<PlayerCommand, 4>& cmds, float dt, float aspect);
// Revives: a standing teammate holds Interact next to a downed player (costs shared gold).
void updateRevives(World& w, const std::array<PlayerCommand, 4>& cmds, float dt);
// Reset per-room combat state (cooldowns kept).
void resetPlayerForRoom(Player& p);
// Recompute stats from relics (max HP etc.).
void applyPlayerStats(Player& p, const PlayerStats& stats);

// The revive cost right now (for UI).
int currentReviveCost(const World& w);

}  // namespace game
