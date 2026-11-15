#pragma once
// Everything alive in the current room plus the services the gameplay systems use.
#include <array>
#include <string>
#include <vector>

#include "engine/audio/audio_manager.h"
#include "engine/core/fixed_clock.h"
#include "engine/core/rng.h"
#include "engine/particles/particles.h"
#include "engine/physics/physics_world.h"
#include "game/core/attack_tokens.h"
#include "game/core/game_state.h"
#include "game/data/game_data.h"
#include "game/rooms/room_instance.h"
#include "game/visual/character_visual.h"
#include "game/world/camera_rig.h"
#include "game/world/entities.h"
#include "game/world/fx_library.h"

namespace game {

struct WorldServices {
    GameData& data;
    GameState& state;
    eng::AssetCache& assets;
    CharacterLibrary& chars;
    eng::PhysicsWorld& phys;
    eng::Particles& particles;
    eng::AudioManager& audio;
    eng::FixedClock& clock;
    const FxLibrary& fx;
};

class World {
public:
    explicit World(const WorldServices& s);
    ~World();

    WorldServices svc;
    std::vector<Player> players;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<ActiveHit> hits;
    std::vector<GoldPickup> gold;
    std::vector<FloatText> texts;
    std::vector<Hazard> hazards;
    std::vector<Chest> chests;
    RoomInstance room;
    const FloorTheme* theme = nullptr;
    AttackTokens tokens;
    CameraRig camera;
    eng::Rng rng;
    float time = 0.f;
    bool screenShake = true;
    Vector3 botGoal{};         // where AI teammates go when there is nothing to fight
    bool botGoalActive = false;

    EntityId nextId() { return ++idCounter_; }
    Player* player(EntityId id);
    Enemy* enemy(EntityId id);
    Player& addPlayer(int slot, const std::string& classId, Vector3 pos, bool dummy);
    Enemy& spawnEnemy(const std::string& enemyId, Vector3 pos, bool boss = false);
    void removeDeadEnemies();
    // Spawns requested during the enemy update are applied afterwards (no vector reallocation mid-loop).
    void queueSpawn(const std::string& enemyId, Vector3 pos) { pendingSpawns_.push_back({enemyId, pos}); }
    void flushSpawns();
    void clearEnemies();
    void placePlayer(Player& p, Vector3 pos);

    void spawnGold(Vector3 pos, int total);
    void text(Vector3 pos, const char* s, Color c, float size);
    void emit(const std::string& fxId, Vector3 pos, Vector3 dir = {0, 1, 0}, float countScale = 1.f);
    void sound(const std::string& id, float volume = 1.f, float pitch = 1.f);
    void hitstop(float seconds);
    void shake(float trauma);

    int playerCount() const { return static_cast<int>(players.size()); }
    int standingPlayers() const;  // alive and not downed
    bool allPlayersDown() const;
    int livingEnemies() const;
    float cfg(const char* section, const char* key) const { return svc.data.cfg(section, key); }
    eng::Hurtbox hurtbox(const Kinematic& k) const { return eng::Hurtbox{k.pos, k.radius, k.height}; }

private:
    EntityId idCounter_ = 0;
    struct PendingSpawn {
        std::string id;
        Vector3 pos;
    };
    std::vector<PendingSpawn> pendingSpawns_;
};

}  // namespace game
