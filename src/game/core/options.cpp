#include "game/core/options.h"

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace game {

bool Options::parse(int argc, char** argv, std::string& err) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                err = std::string("missing value for ") + what;
                return nullptr;
            }
            return argv[++i];
        };
        if (a == "--smoke-test") smokeTest = true;
        else if (a == "--validate") validate = true;
        else if (a == "--verbose") verbose = true;
        else if (a == "--capture") {
            const char* v = next("--capture");
            if (!v) return false;
            captureScene = v;
        } else if (a == "--frames") {
            const char* v = next("--frames");
            if (!v) return false;
            captureFrames = std::atoi(v);
        } else if (a == "--out") {
            const char* v = next("--out");
            if (!v) return false;
            outDir = v;
        } else if (a == "--dummies") {
            const char* v = next("--dummies");
            if (!v) return false;
            dummies = std::atoi(v);
        } else if (a == "--seed") {
            const char* v = next("--seed");
            if (!v) return false;
            seed = std::strtoull(v, nullptr, 10);
        } else if (a == "--size") {
            const char* v = next("--size");
            if (!v) return false;
            width = std::atoi(v);
            const char* x = std::strchr(v, 'x');
            height = x ? std::atoi(x + 1) : 0;
        } else if (a.rfind("-psn_", 0) == 0) {
            // macOS Finder passes a process serial number; ignore it.
        } else {
            err = "unknown argument: " + std::string(a);
            return false;
        }
    }
    return true;
}

}  // namespace game
