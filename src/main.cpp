// Crypt Crew entry point.
#include <cstdio>
#include <string>

#include "game/core/app.h"
#include "game/core/options.h"
#include "game/core/validate.h"
#include "engine/audio/audio_manager.h"
#include "engine/input/input_router.h"
#include "game/core/game_state.h"

int main(int argc, char** argv) {
    game::Options opt;
    std::string err;
    if (!opt.parse(argc, argv, err)) {
        std::fprintf(stderr, "%s\nusage: CryptCrew [--smoke-test] [--validate] [--capture <scene> --frames N --out dir] "
                             "[--dummies N] [--seed S] [--size WxH] [--room ID] [--gold N] [--verbose]\n",
                     err.c_str());
        return 2;
    }
    if (opt.validate) return game::runValidate(opt.verbose);
    // Services created once and passed by reference (no other globals).
    eng::InputRouter input;
    game::GameState state;
    eng::AudioManager audio;
    game::App app(opt, input, state, audio);
    return app.run();
}
