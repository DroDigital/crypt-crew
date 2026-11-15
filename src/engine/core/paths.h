#pragma once
// Locating read-only resources (assets/, data/) and the per-user writable folder.
#include <filesystem>
#include <string>

namespace eng {

// Directory that contains assets/ and data/. Next to the executable, or
// Contents/Resources inside a macOS .app. Falls back to searching parents (dev builds).
const std::filesystem::path& resourceDir();
std::string resPath(const std::string& relative);  // resourceDir()/relative as UTF-8

// UTF-8 string -> filesystem path (std::filesystem::u8path is deprecated in C++20).
inline std::filesystem::path u8path(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

// Per-user writable folder (settings). Created on first call.
const std::filesystem::path& userDataDir();

}  // namespace eng
