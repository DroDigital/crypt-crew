#pragma once
// Player settings persisted in the per-user data folder (settings.json).
#include <string>

namespace game {

struct Settings {
    float master = 1.f, music = 0.7f, sfx = 1.f;
    bool screenShake = true;
    bool fullscreen = false;
    int width = 1600, height = 900;  // windowed resolution
    bool vsync = true;
    bool msaaBackbuffer = true;      // disabled automatically if the driver rejects it

    void load();
    bool save() const;
    static std::string path();
};

}  // namespace game
