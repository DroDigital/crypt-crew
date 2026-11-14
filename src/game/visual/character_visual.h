#pragma once
// Model + animator + anim map + attachments for one on-screen character.
#include <raylib.h>

#include <map>
#include <string>
#include <vector>

#include "engine/anim/animator.h"
#include "engine/assets/asset_cache.h"
#include "engine/render/renderer.h"
#include "game/data/game_data.h"

namespace game {

// Resolves and owns AnimMaps (stable addresses) so visuals can point at them.
class CharacterLibrary {
public:
    CharacterLibrary(eng::AssetCache& assets, const GameData& data) : assets_(assets), data_(data) {}
    // Returns nullptr and logs on failure.
    const AnimMap* animMap(const std::string& animMapPath, const eng::ModelAsset** outModel);
    eng::AssetCache& assets() { return assets_; }
    const GameData& data() const { return data_; }
    void clear() { resolved_.clear(); }

private:
    eng::AssetCache& assets_;
    const GameData& data_;
    std::map<std::string, AnimMap> resolved_;
};

class CharacterVisual {
public:
    bool initHero(CharacterLibrary& lib, const ClassDef& cls);
    bool initEnemy(CharacterLibrary& lib, const EnemyDef& def);

    // Plays a mapped action. fade < 0 uses the default crossfade.
    void play(const std::string& action, bool restart = false, float speedMul = 1.f, float fade = -1.f);
    bool has(const std::string& action) const { return map_ && map_->find(action); }
    const ResolvedAction* action(const std::string& a) const { return map_ ? map_->find(a) : nullptr; }
    const std::string& current() const { return currentAction_; }
    int update(float dt, int* firedEvents);
    bool finished() const { return animator_.finished(); }
    float actionTime() const;  // seconds of real time into the current action

    void draw(eng::Renderer& r, Vector3 pos, float yaw, float flash, Color tint, Vector3 emissive = {0, 0, 0});
    // World position of a joint for the last drawn transform.
    Vector3 jointWorld(const std::string& joint) const;
    float scale = 1.f;
    float defaultFade = 0.12f;

private:
    struct Att {
        const eng::ModelAsset* model = nullptr;
        int joint = -1;
    };
    const eng::ModelAsset* model_ = nullptr;
    const AnimMap* map_ = nullptr;
    eng::Animator animator_;
    uint64_t hidden_ = 0;
    std::vector<Att> attachments_;
    std::string currentAction_;
    float actionSpeed_ = 1.f;
    Matrix lastTransform_{};
    bool initCommon(CharacterLibrary& lib, const std::string& animMapPath);
};

}  // namespace game
