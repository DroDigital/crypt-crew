#pragma once

#include <string>

namespace game {

class Scene {
public:
    virtual ~Scene() = default;
    virtual void fixedUpdate(float dt) = 0;     // 60 Hz simulation step
    virtual void frameUpdate(float /*realDt*/) {}  // per-frame, unscaled (UI, menus)
    virtual void render(float alpha) = 0;       // inside BeginDrawing()
    virtual const char* name() const = 0;
    // Smoke test hook: return false (with a reason) if the simulation state is broken (NaN etc).
    virtual bool healthy(std::string& /*why*/) const { return true; }
};

}  // namespace game
