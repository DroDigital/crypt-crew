#pragma once
// Settings menu (title screen and pause): volumes, screen shake, display mode, resolution,
// vsync. Changes apply immediately and are saved to the user data folder on close.
#include "engine/audio/audio_manager.h"
#include "engine/input/input_router.h"
#include "engine/ui/ui.h"
#include "game/core/settings.h"

namespace game {

class SettingsMenu {
public:
    void open() {
        open_ = true;
        focus_ = 0;
    }
    bool isOpen() const { return open_; }
    // `buttons` are frame edges from whoever controls the menu. Returns true when it closes.
    bool update(uint32_t buttons, Settings& s, eng::AudioManager& audio);
    void draw(const eng::Ui& ui, const Settings& s, eng::PadFamily fam) const;

private:
    void apply(Settings& s, eng::AudioManager& audio) const;
    bool open_ = false;
    int focus_ = 0;
};

}  // namespace game
