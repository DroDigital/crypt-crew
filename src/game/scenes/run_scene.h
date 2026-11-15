#pragma once
// A run through the crypt: rooms, combat, doors, the floor flow, HUD.
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "game/core/app.h"
#include "game/rooms/floor_plan.h"
#include "game/ui/hud.h"
#include "game/rooms/shrine.h"
#include "game/ui/relic_picker.h"
#include "game/ui/settings_menu.h"
#include "game/world/world.h"

namespace game {

struct PlayerSetup {
    int slot = 0;
    std::string classId = "knight";
    bool dummy = false;
};

struct RunConfig {
    std::vector<PlayerSetup> players;
    std::string singleRoom;   // non-empty: play only this room template (M1 / captures)
    bool allBots = false;     // smoke test / capture: every player is AI
    bool reviveDemo = false;  // capture: player 2 starts downed, the team has gold
    uint64_t seed = 1;
};

class RunScene : public Scene {
public:
    RunScene(App& app, const RunConfig& cfg);
    ~RunScene() override;
    void fixedUpdate(float dt) override;
    void frameUpdate(float realDt) override;
    void render(float alpha) override;
    const char* name() const override { return "run"; }
    bool healthy(std::string& why) const override;
    void debugDump() const;
    void debugLines(std::vector<std::string>& out) const override;

    World& world() { return *world_; }

private:
    enum class Phase { Enter, Fight, Cleared, Exit };
    void startFloor(int floor);
    void enterRoom(int index, int fromSide);
    void spawnRoomEnemies();
    void buildCommands(float dt);
    void checkDoors();
    void onRoomCleared();
    void renderWorld(float alpha);
    float aspect() const;

    App& app_;
    RunConfig cfg_;
    std::unique_ptr<World> world_;
    FloorPlan plan_;
    int current_ = 0;
    Phase phase_ = Phase::Enter;
    float phaseTime_ = 0.f;
    int pendingRoom_ = -1, pendingSide_ = -1;
    bool spawned_ = false;
    int wave_ = 0, waves_ = 1;
    float waveDelay_ = -1.f;
    float fade_ = 1.f;
    float bannerTime_ = 0.f;
    std::string banner_;
    int lastPurse_ = 0;
    float goldPulse_ = 0.f;
    float gameOverTime_ = 0.f;
    bool paused_ = false;
    int pauseSlot_ = -1;
    int pauseFocus_ = 0;
    int disconnectedSlot_ = -1;
    bool humanDisconnected() const;
    void drawPause();
    void endRun(bool victory);
    void updateChests(float dt);
    void updateTraps(float dt);
    void nextFloor();
    void updateBotGoal();
    std::string pickPoolEnemy(const std::string& kind);
    RelicPicker picker_;
    int exitSide_ = -1;
    int entrySide_ = -1;  // door the team came in through (bots avoid walking straight back)
    struct Trap {
        Vector3 pos{};
        float phase = 0.f;
    };
    std::vector<Trap> traps_;
    float trapClock_ = 0.f;
    bool floorExit_ = false;
    Shrine shrine_;
    SettingsMenu settingsMenu_;
    bool victory_ = false;
    float victoryTime_ = 0.f;
    float slowmoLeft_ = 0.f;  // real seconds of slow motion left
    void tryDropIn();
    const Enemy* boss() const;
    std::array<PlayerCommand, 4> cmds_{};
};

}  // namespace game
