#pragma once
// Maps gameplay actions (idle, run, attack_1, ...) to clips for one character (data/anims/*.json).
#include <map>
#include <string>
#include <vector>

#include "engine/anim/animator.h"
#include "engine/core/json_util.h"

namespace game {

enum AnimEventId : int { kEventHit = 1 };

struct AnimActionDef {
    std::string clipName;
    float speed = 1.f;
    bool loop = false;
    float hit = -1.f;  // clip time of the hit frame
    float end = -1.f;  // clip time when the action may end (defaults to clip duration)
};

struct AnimMapDef {
    std::string path;
    std::string model;
    std::map<std::string, AnimActionDef> actions;
};

AnimMapDef parseAnimMap(const eng::json& j, const std::string& ctx);

// Resolved against a concrete model's clip list.
struct ResolvedAction {
    int clip = -1;
    float speed = 1.f;
    bool loop = false;
    float hit = -1.f;
    float end = 0.f;  // clip seconds
    float realDuration() const { return speed > 0.f ? end / speed : end; }
    float realHit() const { return speed > 0.f ? hit / speed : hit; }
};

struct AnimMap {
    std::map<std::string, ResolvedAction> actions;
    std::vector<eng::AnimEventDef> events;
    const ResolvedAction* find(const std::string& a) const {
        auto it = actions.find(a);
        return it == actions.end() ? nullptr : &it->second;
    }
};

// Returns false (and fills err) if a clip name is missing from the model.
bool resolveAnimMap(const AnimMapDef& def, const eng::ModelAsset& asset, AnimMap& out, std::string& err);

}  // namespace game
