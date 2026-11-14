#pragma once
// Plain component structs and the entity aggregates built from them.
#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

#include "engine/physics/combat_query.h"
#include "game/visual/character_visual.h"

namespace game {

using EntityId = uint32_t;

// Position/facing with previous-step copies for interpolated rendering.
struct Kinematic {
    Vector3 pos{}, prev{};
    float yaw = 0.f, prevYaw = 0.f;
    Vector3 knock{};  // decaying knockback velocity
    int phys = -1;    // PhysicsWorld character handle
    float radius = 0.5f, height = 2.f;
    Vector3 lerpPos(float a) const {
        return Vector3{prev.x + (pos.x - prev.x) * a, prev.y + (pos.y - prev.y) * a, prev.z + (pos.z - prev.z) * a};
    }
    float lerpYaw(float a) const;
    Vector3 forward() const;
};

struct Health {
    float hp = 1.f, max = 1.f;
    float flash = 0.f;     // hit flash timer
    float iframes = 0.f;   // invulnerable while > 0
    bool alive() const { return hp > 0.f; }
};

// Stat modifiers from relics (data/relics/*.json) applied on top of the class.
struct PlayerStats {
    float moveMult = 1.f, damageMult = 1.f, lifesteal = 0.f, specialCdMult = 1.f, goldMult = 1.f;
    float maxHpBonus = 0.f, reviveSpeedMult = 1.f, thorns = 0.f, aoeMult = 1.f, killHeal = 0.f;
    int extraDodges = 0;
};

enum class PState : uint8_t { Move, Attack, Special, Dodge, Hurt, Downed, Revived, Dead, Cheer };

struct PlayerCommand {
    Vector2 move{0, 0};      // stick/keys, screen-aligned (x right, y down)
    Vector3 aim{0, 0, 0};    // world-space aim direction (zero = auto-aim)
    bool aimExplicit = false;
    uint32_t pressed = 0;    // eng::Button edges
    uint32_t held = 0;
};

struct Player {
    EntityId id = 0;
    int slot = 0;
    bool dummy = false;  // AI teammate (debug)
    const ClassDef* cls = nullptr;
    CharacterVisual vis;
    Kinematic k;
    Health hp;
    PlayerStats stats;
    std::vector<std::string> relicHooks;  // hook ids from relics
    PState state = PState::Move;
    float stateTime = 0.f;
    Vector3 aim{0, 0, 1};
    Vector3 moveDir{0, 0, 0};
    // Combat
    int combo = 0;
    bool comboQueued = false;
    bool hitFired = false;
    float specialCd = 0.f, dodgeCd = 0.f;
    int dodgeCharges = 1;
    float rage = 0.f, guard = 0.f, critWindow = 0.f, shield = 0.f;
    Vector3 dashDir{0, 0, 1};
    Vector3 dashStart{0, 0, 0};
    Vector3 dodgeDir{0, 0, 1};
    // Downed / revive
    float downedTime = 0.f;
    float reviveProgress = 0.f;  // progress being revived (0..1)
    EntityId reviver = 0;
    bool reviveBlockedNoGold = false;
    float fireTrailTimer = 0.f;
    bool downedOrDead() const { return state == PState::Downed || state == PState::Dead; }
    bool canAct() const { return hp.alive() && !downedOrDead(); }
};

enum class EState : uint8_t {
    Spawn, Chase, Windup, Recover, Stagger, Circle, Telegraph, Lunge, Cast, Retreat, Dead,
    BossSlamWindup, BossSlam, BossChargeWindup, BossCharge, BossSummon, BossRoar
};

struct Enemy {
    EntityId id = 0;
    const EnemyDef* def = nullptr;
    CharacterVisual vis;
    Kinematic k;
    Health hp;
    EState state = EState::Spawn;
    float stateTime = 0.f;
    EntityId target = 0;
    std::vector<Vector3> path;
    float repath = 0.f;
    float retarget = 0.f;
    float attackCd = 0.f;
    bool hitFired = false;
    float circleSign = 1.f;
    float circleTime = 0.f;
    Vector3 lungeDir{0, 0, 1};
    float deadTime = 0.f;
    bool isBoss = false;
    int bossCombo = 0;
    bool phase2 = false;
    float moveChoice = 0.f;
    float slamRadius = 0.f;
    EntityId lastHitBy = 0;
    float speedMul = 1.f;
    eng::HitList lungeHits;
    Vector3 strafe{0, 0, 0};
};

enum class ProjKind : uint8_t { Bolt, EnemyBolt, Bomb };

struct Projectile {
    ProjKind kind = ProjKind::Bolt;
    EntityId owner = 0;
    bool fromPlayer = true;
    Vector3 pos{}, prev{}, vel{};
    float radius = 0.4f, damage = 10.f, knock = 2.f, life = 2.f;
    float turnRate = 0.f;  // homing (rad/s)
    EntityId target = 0;
    bool crit = false;
    eng::HitList hits;
};

// A damage volume alive for a few steps; each target is hit at most once.
struct ActiveHit {
    EntityId owner = 0;
    bool fromPlayer = true;
    eng::Hitbox box;
    float damage = 0.f, knock = 0.f;
    bool heavy = false, crit = false, unblockable = false;
    int steps = 2;
    Vector3 knockFrom{};  // knockback pushes away from this point
    eng::HitList hits;
    std::string source;   // for "who died to what"
};

struct GoldPickup {
    Vector3 pos{}, vel{};
    int amount = 1;
    float age = 0.f;
};

struct FloatText {
    Vector3 pos{};
    char text[24]{};
    Color color{255, 255, 255, 255};
    float life = 0.8f, maxLife = 0.8f, size = 26.f;
};

// Ground hazard (fire trail, trap fire, boss shockwave ring).
struct Hazard {
    enum class Kind : uint8_t { FireTrail, TrapFire, ShockRing } kind = Kind::FireTrail;
    Vector3 pos{};
    float radius = 1.f, inner = 0.f, speed = 0.f, maxRadius = 0.f, width = 1.f;
    float life = 1.f, warmup = 0.f, damage = 5.f, tick = 0.f;
    bool fromPlayer = false;
    eng::HitList hits;
};

}  // namespace game
