#pragma once
// Owns the window and the main loop: fixed 60 Hz simulation with interpolated rendering,
// scene switching, capture mode and the smoke test.
#include <memory>
#include <string>

#include "engine/assets/asset_cache.h"
#include "engine/core/fixed_clock.h"
#include "engine/render/renderer.h"
#include "game/core/options.h"
#include "game/core/scene.h"
#include "game/data/game_data.h"
#include "game/visual/character_visual.h"

namespace game {

class App {
public:
    explicit App(const Options& opt);
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

private:
    bool initWindow();
    bool loadAll();
    void preloadAssets();
    int runSmokeTest();
    std::unique_ptr<Scene> makeScene(const std::string& name);
    void saveCapture(int frame);

    Options opt_;
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
