#pragma once
// Debug AI teammates: follow player 1, fight nearby enemies, dodge telegraphs, revive.
#include "game/world/world.h"

namespace game {

PlayerCommand dummyCommand(World& w, Player& self, float dt);

}  // namespace game
