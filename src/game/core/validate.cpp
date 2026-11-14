#include "game/core/validate.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

#include "engine/assets/model_asset.h"
#include "engine/core/log.h"
#include "engine/core/paths.h"
#include "game/data/game_data.h"

namespace fs = std::filesystem;

namespace game {
namespace {

int gChecks = 0;

void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) eng::log::error("validate: %s", what.c_str());
}

bool fileExists(const std::string& rel) { return fs::exists(eng::resourceDir() / rel); }

}  // namespace

int runValidate(bool verbose) {
    eng::log::info("validate: resources at %s", eng::resourceDir().string().c_str());
    // 1) Every glTF/GLB parses.
    int models = 0;
    for (const auto& e : fs::recursive_directory_iterator(eng::resourceDir() / "assets")) {
        const auto ext = e.path().extension().string();
        if (ext != ".glb" && ext != ".gltf") continue;
        std::string err;
        check(eng::validateGltf(e.path().string(), err), err);
        ++models;
    }
    eng::log::info("validate: parsed %d glTF files", models);

    // 2) Every data file loads with its expected fields.
    GameData data;
    try {
        data.loadAll();
    } catch (const std::exception& ex) {
        eng::log::error("validate: data: %s", ex.what());
        return 1;
    }
    eng::log::info("validate: loaded %zu data files", data.loadedFiles.size());

    // 3) Cross references: anim maps vs clips, hidden meshes, attachment joints, shaders.
    for (const auto& [path, def] : data.animMaps) {
        check(fileExists(def.model), path + ": model missing: " + def.model);
        eng::ModelAsset asset;
        asset.path = def.model;
        std::string err;
        if (!eng::loadSkeletonAndClips(eng::resPath(def.model), asset, err)) {
            check(false, err);
            continue;
        }
        for (const auto& [action, a] : def.actions) {
            const int clip = asset.findClip(a.clipName);
            check(clip >= 0, path + ": clip '" + a.clipName + "' not in " + def.model);
            if (clip >= 0 && a.hit >= 0.f)
                check(a.hit <= asset.clips[static_cast<size_t>(clip)].duration,
                      path + ": hit time past clip end for " + action);
        }
    }
    for (const auto& [id, c] : data.classes) {
        const auto& def = data.animMap(c.animMap);
        std::vector<std::string> meshes;
        std::string err;
        check(eng::listMeshNodeNames(eng::resPath(def.model), meshes, err), err);
        for (const auto& h : c.hideMeshes)
            check(std::find(meshes.begin(), meshes.end(), h) != meshes.end(),
                  "class " + id + ": hide_meshes '" + h + "' not a mesh node");
        for (const auto& step : c.combo)
            check(def.actions.count(step.anim) > 0, "class " + id + ": combo anim '" + step.anim + "' not mapped");
    }
    for (const auto& [id, e] : data.enemies) {
        const auto& def = data.animMap(e.animMap);
        eng::ModelAsset asset;
        std::string err;
        eng::loadSkeletonAndClips(eng::resPath(def.model), asset, err);
        for (const auto& a : e.attachments) {
            check(fileExists(a.model), "enemy " + id + ": attachment missing: " + a.model);
            check(asset.skeleton.find(a.joint) >= 0, "enemy " + id + ": joint '" + a.joint + "' not found");
        }
        check(def.actions.count(e.attack.anim) > 0, "enemy " + id + ": attack anim not mapped");
    }
    for (const char* sh : {"lit.vs", "lit.fs", "particle.vs", "particle.fs", "bright.fs", "blur.fs", "composite.fs"})
        check(fileExists(std::string("assets/shaders/") + sh), std::string("shader missing: ") + sh);

    const int errors = eng::log::errorCount();
    eng::log::info("validate: %d checks, %d error(s)%s", gChecks, errors, verbose ? "" : "");
    return errors == 0 ? 0 : 1;
}

}  // namespace game
