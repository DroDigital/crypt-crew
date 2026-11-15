#pragma once
#include <cstdint>
#include <string>

namespace game {

struct Options {
    bool smokeTest = false;
    bool validate = false;
    bool verbose = false;
    std::string captureScene;  // gallery | room:<id> | boss | hud
    int captureFrames = 90;
    int captureEvery = 0;      // also save every N frames (0 = only N/2 and N)
    std::string outDir = "shots";
    int dummies = -1;          // AI teammates following P1 (-1 = data/config.json)
    uint64_t seed = 0;         // 0 = random
    std::string room;          // debug: start in this room template as player 1 (keyboard/pad)
    int gold = 0;              // debug: starting gold
    int steps = 3600;          // smoke test length in fixed steps
    int width = 0, height = 0; // window override
    bool parse(int argc, char** argv, std::string& err);
};

}  // namespace game
