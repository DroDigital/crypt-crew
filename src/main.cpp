// Crypt Crew entry point.
#include <cstdio>
#include <string>

#include "game/core/app.h"
#include "game/core/options.h"
#include "game/core/validate.h"

int main(int argc, char** argv) {
    game::Options opt;
    std::string err;
    if (!opt.parse(argc, argv, err)) {
        std::fprintf(stderr, "%s\nusage: CryptCrew [--smoke-test] [--validate] [--capture <scene> --frames N --out dir] "
                             "[--dummies N] [--seed S] [--size WxH] [--verbose]\n",
                     err.c_str());
        return 2;
    }
    if (opt.validate) return game::runValidate(opt.verbose);
    game::App app(opt);
    return app.run();
}
