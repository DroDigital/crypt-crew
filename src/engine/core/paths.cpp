#include "engine/core/paths.h"

#include <cstdlib>
#include <raylib.h>

namespace fs = std::filesystem;

namespace eng {
namespace {

bool hasResources(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p / "data", ec) && fs::is_directory(p / "assets", ec);
}

fs::path findResourceDir() {
    const fs::path exeDir = fs::path(GetApplicationDirectory());
    // macOS bundle: <App>.app/Contents/MacOS/<exe> -> Contents/Resources
    const fs::path bundleRes = exeDir / ".." / "Resources";
    if (hasResources(bundleRes)) return fs::weakly_canonical(bundleRes);
    if (hasResources(exeDir)) return exeDir;
    // Dev builds: build/<preset>/ -> walk up to the source tree.
    fs::path p = exeDir;
    for (int i = 0; i < 5 && p.has_parent_path(); ++i) {
        p = p.parent_path();
        if (hasResources(p)) return p;
    }
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec && hasResources(cwd)) return cwd;
    return exeDir;
}

fs::path findUserDir() {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    fs::path base = appdata ? fs::path(appdata) : fs::temp_directory_path();
    return base / "CryptCrew";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : "/tmp") / "Library" / "Application Support" / "CryptCrew";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return fs::path(xdg) / "crypt-crew";
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : "/tmp") / ".local" / "share" / "crypt-crew";
#endif
}

}  // namespace

const fs::path& resourceDir() {
    static const fs::path dir = findResourceDir();
    return dir;
}

std::string resPath(const std::string& relative) { return (resourceDir() / relative).generic_string(); }

const fs::path& userDataDir() {
    static const fs::path dir = [] {
        fs::path d = findUserDir();
        std::error_code ec;
        fs::create_directories(d, ec);
        return d;
    }();
    return dir;
}

}  // namespace eng
