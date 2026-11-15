#pragma once
// Relic hook dispatch. Relics are data (data/relics/*.json): stat modifiers plus an optional
// hook id. Hooks are C++ functions registered by id in one table (relic_hooks.cpp) and called
// from gameplay events. No scripting language.
#include <string>
#include <vector>

namespace game {

class World;
struct Player;
struct Enemy;

namespace hooks {

bool exists(const std::string& id);
std::vector<std::string> registeredIds();

void onHit(World& w, Player& p, Enemy& e, float damage, bool crit);
void onKill(World& w, Player& p, Enemy& e);
void onDodge(World& w, Player& p);
void onRevive(World& w, Player& reviver, Player& revived);
void onRoomClear(World& w);
// Queries used by combat/abilities.
bool has(const Player& p, const std::string& id);

}  // namespace hooks
}  // namespace game
