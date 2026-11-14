#pragma once
// Damage resolution and the per-step combat systems (hitboxes, projectiles, hazards, gold).
#include <string>

#include "game/world/world.h"

namespace game {

struct DamageInfo {
    float amount = 0.f;
    Vector3 from{};          // knockback origin (attacker position)
    float knock = 0.f;
    bool heavy = false, crit = false, unblockable = false, fromPlayer = true;
    EntityId attacker = 0;
    std::string source;      // e.g. "Skeleton Minion" for the run summary
};

// Returns true if damage was dealt (false when blocked/ignored).
bool damageEnemy(World& w, Enemy& e, DamageInfo d);
void damagePlayer(World& w, Player& p, DamageInfo d);
void healPlayer(World& w, Player& p, float amount, bool showText = true);
void killEnemy(World& w, Enemy& e, Player* killer);
void setPlayerDowned(World& w, Player& p, const std::string& source);

void spawnHit(World& w, const ActiveHit& h);
void updateHits(World& w);
void updateProjectiles(World& w, float dt);
void updateHazards(World& w, float dt);
void updateGold(World& w, float dt);
void updateTexts(World& w, float dt);
void updateKnockback(Kinematic& k, float decay, float dt);

}  // namespace game
