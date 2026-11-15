#pragma once
// All tunable game data, loaded from data/*.json at startup (F5 hot-reloads in debug builds).
#include <raylib.h>

#include <array>
#include <map>
#include <string>
#include <vector>

#include "engine/core/json_util.h"
#include "engine/render/renderer.h"
#include "game/data/anim_map.h"
#include "game/rooms/room_template.h"

namespace game {

struct AttackStep {
    std::string anim;
    float damage = 0, range = 0, arcDeg = 0, knockback = 0, lunge = 0;
    float speed = 0, radius = 0;  // projectiles
    bool heavy = false;
};

struct ClassDef {
    std::string id, name, description, animMap;
    std::vector<std::string> hideMeshes;
    Color color{255, 255, 255, 255};
    float hp = 100, moveSpeed = 6, radius = 0.5f, height = 2.f;
    std::string basicType;  // melee | projectile
    float comboWindow = 0.4f, moveDuring = 0.3f;
    std::vector<AttackStep> combo;
    std::string specialType;
    float specialCooldown = 8.f;
    eng::json special;  // type-specific parameters
};

struct Attachment {
    std::string model, joint;
};

struct EnemyDef {
    std::string id, name, animMap, behavior;
    std::vector<Attachment> attachments;
    float hp = 30, moveSpeed = 4, radius = 0.5f, height = 2.f, scale = 1.f;
    Color tint{255, 255, 255, 255};
    AttackStep attack;
    float attackCooldown = 1.5f, triggerRange = 2.f;
    std::array<int, 2> gold{1, 2}, group{1, 1};
    float staggerTime = 0.3f, knockbackResist = 0.f;
    eng::json raw;  // behavior-specific parameters
};

struct ScalingDef {
    std::vector<float> enemyHpMult{1.f, 1.5f, 1.9f, 2.3f};
    float spawnBonusPerPlayer = 0.3f;
    std::vector<float> enemyDamageMult{1.f, 1.f, 1.f, 1.f};
    float damageMult(int players) const;
    float hpMult(int players) const;
    int spawnCount(int base, int players) const;
};

struct FloorTheme {
    std::string id, name;
    eng::Lighting lighting;
    Vector3 torchColor{2, 1.2f, 0.6f};
    float torchRadius = 10.f;
    Vector3 fillColor{0.2f, 0.2f, 0.25f};
    std::string banner;
    std::array<int, 2> rooms{6, 8};
    std::vector<std::string> enemies;
    float eliteChance = 0.f;
    int waves = 1;               // enemy waves per combat room
    float enemyHpMult = 1.f;     // on top of player-count scaling
    float enemyDamageMult = 1.f;
};

struct RelicDef {
    std::string id, name, description, glyph;
    Color color{255, 255, 255, 255};
    std::map<std::string, float> add;   // additive stats: "move_mult": 0.15, "max_hp_bonus": 20, ...
    std::string hook;                   // optional hook id (relic_hooks.cpp)
    std::string classOnly;              // e.g. "mage" (empty = any class)
    bool stackable = true;
    int price = 60;                     // shrine merchant
};

class GameData {
public:
    // Loads everything; throws std::runtime_error with file context on any problem.
    void loadAll();
    // Re-reads all JSON and updates existing entries in place (pointers stay valid).
    bool reload(std::string& err);

    eng::json config;
    std::map<std::string, ClassDef> classes;
    std::vector<std::string> classOrder;  // knight, barbarian, mage, rogue
    std::map<std::string, EnemyDef> enemies;
    std::map<std::string, AnimMapDef> animMaps;  // keyed by path
    ScalingDef scaling;
    std::vector<FloorTheme> floors;
    std::map<std::string, RoomTemplate> rooms;
    std::map<std::string, RelicDef> relics;
    std::vector<std::string> loadedFiles;

    float cfg(const char* section, const char* key) const;
    const ClassDef& cls(const std::string& id) const;
    const EnemyDef& enemy(const std::string& id) const;
    const AnimMapDef& animMap(const std::string& path) const;
    const RoomTemplate& room(const std::string& id) const;
};

}  // namespace game
