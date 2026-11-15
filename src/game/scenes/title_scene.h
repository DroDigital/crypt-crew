#pragma once
// Title screen: the four heroes in a torch-lit crypt, version, Play / Settings / Quit.
#include <array>

#include "game/core/app.h"
#include "game/ui/settings_menu.h"
#include "game/visual/character_visual.h"

namespace game {

class TitleScene : public Scene {
public:
    explicit TitleScene(App& app);
    void fixedUpdate(float dt) override;
    void frameUpdate(float realDt) override;
    void render(float alpha) override;
    const char* name() const override { return "title"; }

private:
    App& app_;
    std::array<CharacterVisual, 4> heroes_;
    float time_ = 0.f;
    int focus_ = 0;
    int lastDevice_ = 0;
    SettingsMenu settings_;
};

}  // namespace game
