#include "game/data/anim_map.h"

namespace game {

AnimMapDef parseAnimMap(const eng::json& j, const std::string& ctx) {
    AnimMapDef d;
    d.path = ctx;
    d.model = eng::req<std::string>(j, "model", ctx);
    const auto& acts = j.at("actions");
    for (auto it = acts.begin(); it != acts.end(); ++it) {
        AnimActionDef a;
        const std::string c = ctx + ":" + it.key();
        a.clipName = eng::req<std::string>(*it, "clip", c);
        a.speed = eng::opt(*it, "speed", 1.f);
        a.loop = eng::opt(*it, "loop", false);
        a.hit = eng::opt(*it, "hit", -1.f);
        a.end = eng::opt(*it, "end", -1.f);
        d.actions[it.key()] = a;
    }
    for (const char* required : {"idle", "run", "hit", "death"})
        if (!d.actions.count(required)) throw std::runtime_error(ctx + ": missing action '" + required + "'");
    return d;
}

bool resolveAnimMap(const AnimMapDef& def, const eng::ModelAsset& asset, AnimMap& out, std::string& err) {
    out.actions.clear();
    out.events.clear();
    for (const auto& [name, a] : def.actions) {
        ResolvedAction r;
        r.clip = asset.findClip(a.clipName);
        if (r.clip < 0) {
            err = def.path + ": clip '" + a.clipName + "' (action " + name + ") not found in " + asset.path;
            return false;
        }
        const float dur = asset.clips[static_cast<size_t>(r.clip)].duration;
        r.speed = a.speed;
        r.loop = a.loop;
        r.hit = a.hit;
        r.end = a.end > 0.f ? std::min(a.end, dur) : dur;
        out.actions[name] = r;
        if (a.hit >= 0.f) out.events.push_back(eng::AnimEventDef{r.clip, a.hit, kEventHit});
    }
    return true;
}

}  // namespace game
