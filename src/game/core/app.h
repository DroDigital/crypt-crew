#pragma once
// Owns the window and the main loop: fixed 60 Hz simulation with interpolated rendering,
// scene switching, capture mode and the smoke test.
#include <memory>
#include <string>

#include "engine/assets/asset_cache.h"
#include "engine/audio/audio_manager.h"
#include "engine/input/input_devices.h"
#include "engine/input/input_router.h"
#include "engine/particles/particles.h"
#include "engine/physics/physics_world.h"
#include "engine/ui/ui.h"
#include "game/core/game_state.h"
#include "game/world/fx_library.h"
#include "engine/core/fixed_clock.h"
#include "engine/render/renderer.h"
#include "game/core/options.h"
#include "game/core/settings.h"
#include "game/core/scene.h"
#include "game/data/game_data.h"
#include "game/visual/character_visual.h"

namespace game {

class App {
public:
    App(const Options& opt, eng::InputRouter& input, GameState& state, eng::AudioManager& audio);
    int run();

    // Scene switching takes effect at the start of the next frame.
    void switchTo(std::unique_ptr<Scene> next) { pending_ = std::move(next); }
    void quit() { quit_ = true; }

    const Options& options() const { return opt_; }
    GameData& data() { return data_; }
    eng::AssetCache& assets() { return assets_; }
    eng::Renderer& renderer() { return renderer_; }
    eng::FixedClock& clock() { return clock_; }
    CharacterLibrary& characters() { return chars_; }
    double simTime() const { return simTime_; }
    eng::InputRouter& input() { return input_; }
    GameState& state() { return state_; }
    eng::AudioManager& audio() { return audio_; }
    eng::PhysicsWorld& physics() { return physics_; }
    eng::Particles& particles() { return particles_; }
    const FxLibrary& fx() const { return fx_; }
    eng::Ui& ui() { return ui_; }
    Settings& settings() { return settings_; }
    void applySettings();
    bool offline() const { return opt_.smokeTest || !opt_.captureScene.empty(); }
    std::unique_ptr<Scene> makeScene(const std::string& name);

private:
    bool initWindow();
    bool loadAll();
    void preloadAssets();
    int runSmokeTest();
    void saveCapture(int frame);
    void drawDebugOverlay();
    bool debugOverlay_ = false;
    bool loadAudio();
    void shutdownAll();

    Options opt_;
    eng::InputRouter& input_;
    GameState& state_;
    eng::AudioManager& audio_;
    eng::PhysicsWorld physics_;
    eng::Particles particles_;
    FxLibrary fx_;
    eng::Ui ui_;
    Settings settings_;
    GameData data_;
    eng::AssetCache assets_;
    eng::Renderer renderer_;
    eng::FixedClock clock_;
    CharacterLibrary chars_{assets_, data_};
    std::unique_ptr<Scene> scene_, pending_;
    bool quit_ = false;
    double simTime_ = 0.0;
};

}  // namespace game
