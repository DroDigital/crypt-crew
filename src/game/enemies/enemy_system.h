#pragma once
#include "game/world/world.h"

namespace game {

void updateEnemies(World& w, float dt);
// Shared helpers (also used by the boss).
Player* pickTarget(World& w, Enemy& e, float dt);
Vector3 steerTo(World& w, Enemy& e, Vector3 goal, float dt);  // unit direction with separation
void moveEnemy(World& w, Enemy& e, Vector3 vel, float dt);
void faceToward(Enemy& e, Vector3 dir, float maxRadPerSec, float dt);
void enemyMeleeHit(World& w, Enemy& e, float damageMult = 1.f, float rangeMult = 1.f);
// Telegraph strength 0..1 for rendering the weapon flash.
float telegraphGlow(const Enemy& e);

// Implemented in boss/boss_ai.cpp.
void updateBoss(World& w, Enemy& e, float dt);

}  // namespace game
