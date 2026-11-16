#include "game/core/validate.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

#include <raylib.h>

#include "engine/assets/model_asset.h"
#include "engine/core/log.h"
#include "engine/core/paths.h"
#include "game/data/game_data.h"
#include "game/data/relic_defs.h"
#include "game/rooms/room_instance.h"
#include "game/world/fx_library.h"
#include "game/world/relic_hooks.h"

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
    // Rooms: every model referenced by legends and props exists.
    for (const auto& [id, r] : data.rooms) {
        for (const auto& [c, e] : r.legend) {
            if (!e.floor.empty()) check(fileExists(dungeonModelPath(e.floor)), r.file + ": floor model " + e.floor);
            if (!e.prop.empty()) check(fileExists(dungeonModelPath(e.prop)), r.file + ": prop model " + e.prop);
        }
        for (const auto& p : r.props) check(fileExists(dungeonModelPath(p.model)), r.file + ": prop model " + p.model);
    }
    for (const auto& f : data.floors)
        if (!f.banner.empty()) check(fileExists(dungeonModelPath(f.banner)), "floor " + f.id + ": banner " + f.banner);
    // Relics: hooks registered in C++, class restrictions valid.
    for (const auto& [id, r] : data.relics) {
        if (!r.hook.empty()) check(hooks::exists(r.hook), "relic " + id + ": unknown hook '" + r.hook + "'");
        if (!r.classOnly.empty()) check(data.classes.count(r.classOnly) > 0, "relic " + id + ": unknown class " + r.classOnly);
    }
    // Audio, font, particle presets.
    try {
        const eng::json a = eng::loadJsonFile("data/audio.json");
        // Every sound must exist and decode (no audio device needed for LoadWave).
        auto decodes = [](const std::string& rel) {
            if (!fileExists(rel)) return false;
            Wave w = LoadWave(eng::resPath(rel).c_str());
            const bool ok = w.frameCount > 0 && w.sampleRate > 0;
            UnloadWave(w);
            return ok;
        };
        for (auto it = a.at("sfx").begin(); it != a.at("sfx").end(); ++it)
            for (const auto& f : it->at("files")) check(decodes(f.get<std::string>()), "sfx file " + f.get<std::string>());
        for (auto it = a.at("music").begin(); it != a.at("music").end(); ++it)
            check(decodes(it->get<std::string>()), "music file " + it->get<std::string>());
        const eng::json ui = eng::loadJsonFile("data/ui.json");
        check(fileExists(ui.at("font").get<std::string>()), "ui font");
        for (const char* fam : {"xbox", "playstation", "switch", "keyboard"})
            for (const char* act : {"attack", "special", "dodge", "interact", "pause", "confirm", "back"})
                check(ui.at("glyphs").at(fam).contains(act) && fileExists(ui.at("glyphs").at(fam).at(act).get<std::string>()),
                      std::string("ui glyph ") + fam + "." + act);
        FxLibrary fx;
        fx.load();
        check(fx.size() > 0, "fx presets");
    } catch (const std::exception& ex) {
        check(false, ex.what());
    }
    for (const char* sh : {"lit.vs", "lit.fs", "particle.vs", "particle.fs", "bright.fs", "blur.fs", "composite.fs"})
        check(fileExists(std::string("assets/shaders/") + sh), std::string("shader missing: ") + sh);

    const int errors = eng::log::errorCount();
    eng::log::info("validate: %d checks, %d error(s)%s", gChecks, errors, verbose ? "" : "");
    return errors == 0 ? 0 : 1;
}

}  // namespace game
