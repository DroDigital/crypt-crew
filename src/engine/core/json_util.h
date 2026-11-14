#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace eng {
using json = nlohmann::json;

// Loads a JSON file (relative to resourceDir). Throws std::runtime_error with the path on failure.
json loadJsonFile(const std::string& relativePath);
json loadJsonAbsolute(const std::string& absolutePath);

// Required field access with a readable error ("data/classes/knight.json: missing 'hp'").
template <typename T>
T req(const json& j, const char* key, const std::string& ctx) {
    if (!j.contains(key)) throw std::runtime_error(ctx + ": missing field '" + key + "'");
    try {
        return j.at(key).get<T>();
    } catch (const std::exception& e) {
        throw std::runtime_error(ctx + ": bad field '" + key + "': " + e.what());
    }
}
template <typename T>
T opt(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    return it == j.end() ? fallback : it->get<T>();
}

}  // namespace eng
