#include "game/world/fx_library.h"

#include <array>

#include "engine/core/json_util.h"

namespace game {
namespace {
Color col(const eng::json& j, const char* key, const std::string& ctx) {
    const auto a = eng::req<std::array<int, 4>>(j, key, ctx);
    return Color{static_cast<unsigned char>(a[0]), static_cast<unsigned char>(a[1]), static_cast<unsigned char>(a[2]),
                 static_cast<unsigned char>(a[3])};
}
}  // namespace

void FxLibrary::load() {
    const std::string file = "data/fx.json";
    const eng::json j = eng::loadJsonFile(file);
    emitters_.clear();
    for (auto it = j.at("emitters").begin(); it != j.at("emitters").end(); ++it) {
        const std::string ctx = file + ":" + it.key();
        const auto& v = *it;
        eng::EmitterDef e;
        e.count = eng::req<int>(v, "count", ctx);
        const auto sp = eng::req<std::array<float, 2>>(v, "speed", ctx);
        e.speedMin = sp[0];
        e.speedMax = sp[1];
        e.spreadDeg = eng::req<float>(v, "spread_deg", ctx);
        e.upBias = eng::opt(v, "up", 0.f);
        const auto life = eng::req<std::array<float, 2>>(v, "life", ctx);
        e.lifeMin = life[0];
        e.lifeMax = life[1];
        const auto size = eng::req<std::array<float, 2>>(v, "size", ctx);
        e.sizeStart = size[0];
        e.sizeEnd = size[1];
        e.colorStart = col(v, "color_start", ctx);
        e.colorEnd = col(v, "color_end", ctx);
        e.gravity = eng::opt(v, "gravity", 0.f);
        e.drag = eng::opt(v, "drag", 1.f);
        e.jitter = eng::opt(v, "jitter", 0.f);
        e.additive = eng::opt(v, "additive", true);
        emitters_[it.key()] = e;
    }
}

const eng::EmitterDef* FxLibrary::find(const std::string& id) const {
    auto it = emitters_.find(id);
    return it == emitters_.end() ? nullptr : &it->second;
}

}  // namespace game
