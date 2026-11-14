#include "game/visual/character_visual.h"

#include <raymath.h>

#include "engine/core/log.h"

namespace game {

const AnimMap* CharacterLibrary::animMap(const std::string& path, const eng::ModelAsset** outModel) {
    const AnimMapDef& def = data_.animMap(path);
    const eng::ModelAsset* model = assets_.model(def.model);
    if (outModel) *outModel = model;
    auto it = resolved_.find(path);
    if (it != resolved_.end()) return &it->second;
    AnimMap m;
    std::string err;
    if (!model || model->model.meshCount == 0 || !resolveAnimMap(def, *model, m, err)) {
        eng::log::error("anim map %s: %s", path.c_str(), err.empty() ? "model failed to load" : err.c_str());
        return nullptr;
    }
    return &(resolved_[path] = std::move(m));
}

bool CharacterVisual::initCommon(CharacterLibrary& lib, const std::string& animMapPath) {
    map_ = lib.animMap(animMapPath, &model_);
    if (!map_ || !model_) return false;
    animator_.init(model_);
    animator_.setEvents(&map_->events);
    defaultFade = lib.data().cfg("anim", "crossfade");
    lastTransform_ = MatrixIdentity();
    play("idle", true, 1.f, 0.f);
    return true;
}

bool CharacterVisual::initHero(CharacterLibrary& lib, const ClassDef& cls) {
    if (!initCommon(lib, cls.animMap)) return false;
    hidden_ = 0;
    for (const auto& name : cls.hideMeshes) {
        const int idx = model_->findMesh(name);
        if (idx < 0) {
            eng::log::error("class %s: hide_meshes entry '%s' not found", cls.id.c_str(), name.c_str());
            continue;
        }
        if (idx < 64) hidden_ |= (1ULL << idx);
    }
    return true;
}

bool CharacterVisual::initEnemy(CharacterLibrary& lib, const EnemyDef& def) {
    if (!initCommon(lib, def.animMap)) return false;
    scale = def.scale;
    attachments_.clear();
    for (const auto& a : def.attachments) {
        Att att;
        att.model = lib.assets().model(a.model);
        att.joint = model_->skeleton.find(a.joint);
        if (att.joint < 0) eng::log::error("enemy %s: joint '%s' not found", def.id.c_str(), a.joint.c_str());
        attachments_.push_back(att);
    }
    return true;
}

void CharacterVisual::play(const std::string& action, bool restart, float speedMul, float fade) {
    if (!map_) return;
    const ResolvedAction* a = map_->find(action);
    if (!a) return;
    if (action == currentAction_ && !restart) {
        animator_.setSpeed(a->speed * speedMul);
        return;
    }
    currentAction_ = action;
    actionSpeed_ = a->speed * speedMul;
    animator_.play(a->clip, eng::PlayParams{actionSpeed_, a->loop, fade < 0.f ? defaultFade : fade, true});
}

float CharacterVisual::actionTime() const { return actionSpeed_ > 0.f ? animator_.time() / actionSpeed_ : 0.f; }

int CharacterVisual::update(float dt, int* firedEvents) { return animator_.update(dt, firedEvents); }

void CharacterVisual::draw(eng::Renderer& r, Vector3 pos, float yaw, float flash, Color tint, Vector3 emissive) {
    if (!model_) return;
    animator_.evaluate();
    lastTransform_ = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(yaw)),
                                    MatrixTranslate(pos.x, pos.y, pos.z));
    eng::DrawParams p;
    p.skin = animator_.skinMatrices();
    p.jointCount = animator_.jointCount();
    p.tint = tint;
    p.flash = flash;
    p.emissive = emissive;
    p.hiddenMeshes = hidden_;
    r.drawModel(*model_, lastTransform_, p);
    eng::DrawParams ap;
    ap.tint = tint;
    ap.flash = flash;
    ap.emissive = emissive;
    for (const Att& a : attachments_) {
        if (!a.model || a.joint < 0) continue;
        r.drawModel(*a.model, MatrixMultiply(animator_.jointModel(a.joint), lastTransform_), ap);
    }
}

Vector3 CharacterVisual::jointWorld(const std::string& joint) const {
    if (!model_) return Vector3{0, 0, 0};
    const int j = model_->skeleton.find(joint);
    return Vector3Transform(Vector3{0, 0, 0}, MatrixMultiply(animator_.jointModel(j), lastTransform_));
}

}  // namespace game
