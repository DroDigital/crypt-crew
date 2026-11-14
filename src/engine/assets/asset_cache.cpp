#include "engine/assets/asset_cache.h"

#include <filesystem>
#include <rlgl.h>

#include "engine/core/log.h"
#include "engine/core/paths.h"

namespace eng {

AssetCache::~AssetCache() { unloadAll(); }

void AssetCache::unloadAll() {
    if (!IsWindowReady()) {
        models_.clear();
        textures_.clear();
        return;
    }
    for (auto& [k, m] : models_) {
        // Shared textures are owned by textures_; detach before UnloadModel frees them.
        for (int i = 0; i < m->model.materialCount; ++i) {
            for (auto& [tk, t] : textures_)
                if (m->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id == t.id)
                    m->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{};
        }
        UnloadModel(m->model);
    }
    models_.clear();
    for (auto& [k, t] : textures_) UnloadTexture(t);
    textures_.clear();
}

Texture2D AssetCache::texture(const std::string& path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) return it->second;
    const std::string full = resPath(path);
    Texture2D t{};
    if (std::filesystem::exists(eng::u8path(full))) {
        t = LoadTexture(full.c_str());
        if (t.id != 0) {
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        }
    }
    if (t.id == 0) {
        ++failures_;
        failed_.push_back(path);
        log::error("missing texture: %s", path.c_str());
    }
    textures_[path] = t;
    return t;
}

const ModelAsset* AssetCache::find(const std::string& path) const {
    auto it = models_.find(path);
    return it == models_.end() ? nullptr : it->second.get();
}

const ModelAsset* AssetCache::model(const std::string& path, const std::string& sharedTexture) {
    auto it = models_.find(path);
    if (it != models_.end()) return it->second.get();
    auto asset = std::make_unique<ModelAsset>();
    asset->path = path;
    const std::string full = resPath(path);
    if (!std::filesystem::exists(eng::u8path(full))) {
        ++failures_;
        failed_.push_back(path);
        log::error("missing model: %s", path.c_str());
        models_[path] = std::move(asset);
        return models_[path].get();
    }
    asset->model = LoadModel(full.c_str());
    if (asset->model.meshCount == 0) {
        ++failures_;
        failed_.push_back(path);
        log::error("failed to load model: %s", path.c_str());
    } else {
        std::string err;
        if (!loadGltfExtras(full, *asset, err)) {
            ++failures_;
            failed_.push_back(path);
            log::error("%s", err.c_str());
        }
        asset->bounds = GetModelBoundingBox(asset->model);
        if (!sharedTexture.empty()) {
            Texture2D shared = texture(sharedTexture);
            for (int i = 0; i < asset->model.materialCount; ++i) {
                Texture2D& t = asset->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
                if (t.id != 0 && t.id != shared.id && t.id != rlGetTextureIdDefault()) UnloadTexture(t);
                t = shared;
            }
        } else {
            for (int i = 0; i < asset->model.materialCount; ++i) {
                Texture2D& t = asset->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
                if (t.id != 0 && t.id != rlGetTextureIdDefault()) {
                    GenTextureMipmaps(&t);
                    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
                }
            }
        }
    }
    ModelAsset* raw = asset.get();
    models_[path] = std::move(asset);
    return raw;
}

}  // namespace eng
