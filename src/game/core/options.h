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
    std::string outDir = "shots";
    int dummies = -1;          // AI teammates following P1 (-1 = data/config.json)
    uint64_t seed = 0;         // 0 = random
    int width = 0, height = 0; // window override
    bool parse(int argc, char** argv, std::string& err);
};

}  // namespace game
