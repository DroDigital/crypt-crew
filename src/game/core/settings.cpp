#include "game/core/settings.h"

#include <fstream>

#include "engine/core/json_util.h"
#include "engine/core/log.h"
#include "engine/core/paths.h"

namespace game {

std::string Settings::path() { return (eng::userDataDir() / "settings.json").string(); }

void Settings::load() {
    try {
        const eng::json j = eng::loadJsonAbsolute(path());
        master = j.value("master", master);
        music = j.value("music", music);
        sfx = j.value("sfx", sfx);
        screenShake = j.value("screen_shake", screenShake);
        fullscreen = j.value("fullscreen", fullscreen);
        width = j.value("width", width);
        height = j.value("height", height);
        vsync = j.value("vsync", vsync);
        msaaBackbuffer = j.value("msaa_backbuffer", msaaBackbuffer);
    } catch (const std::exception&) {
        // First launch or unreadable file: defaults.
    }
}

bool Settings::save() const {
    eng::json j;
    j["master"] = master;
    j["music"] = music;
    j["sfx"] = sfx;
    j["screen_shake"] = screenShake;
    j["fullscreen"] = fullscreen;
    j["width"] = width;
    j["height"] = height;
    j["vsync"] = vsync;
    j["msaa_backbuffer"] = msaaBackbuffer;
    std::ofstream out(eng::u8path(path()));
    if (!out) {
        eng::log::warn("settings: cannot write %s", path().c_str());
        return false;
    }
    out << j.dump(2) << "\n";
    return true;
}

}  // namespace game
