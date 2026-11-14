#include "engine/core/json_util.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "engine/core/paths.h"

namespace eng {

json loadJsonAbsolute(const std::string& path) {
    std::ifstream in(eng::u8path(path));
    if (!in) throw std::runtime_error("cannot open " + path);
    try {
        return json::parse(in, nullptr, true, /*ignore_comments=*/true);
    } catch (const std::exception& e) {
        throw std::runtime_error(path + ": " + e.what());
    }
}

json loadJsonFile(const std::string& rel) { return loadJsonAbsolute(resPath(rel)); }

}  // namespace eng
