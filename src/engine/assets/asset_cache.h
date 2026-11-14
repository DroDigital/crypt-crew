#pragma once
// Loads and caches models, textures, fonts and shaders. Everything is loaded up front;
// gameplay only looks things up.
#include <raylib.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/assets/model_asset.h"

namespace eng {

class AssetCache {
public:
    AssetCache() = default;
    AssetCache(const AssetCache&) = delete;
    AssetCache& operator=(const AssetCache&) = delete;
    ~AssetCache();

    // Paths are relative to the resource dir. sharedTexture (optional, relative path) replaces
    // every material's diffuse texture so all pieces share one GPU copy of the atlas.
    const ModelAsset* model(const std::string& path, const std::string& sharedTexture = "");
    const ModelAsset* find(const std::string& path) const;
    Texture2D texture(const std::string& path);
    int failures() const { return failures_; }
    const std::vector<std::string>& failedPaths() const { return failed_; }
    void unloadAll();

private:
    std::map<std::string, std::unique_ptr<ModelAsset>> models_;
    std::map<std::string, Texture2D> textures_;
    std::vector<std::string> failed_;
    int failures_ = 0;
};

}  // namespace eng
