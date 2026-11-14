#pragma once
// Pooled CPU particles drawn as instanced camera-facing billboards, with a global cap.
#include <raylib.h>

#include <string>
#include <vector>

#include "engine/core/rng.h"
#include "engine/render/renderer.h"

namespace eng {

struct EmitterDef {
    int count = 10;
    float speedMin = 1.f, speedMax = 4.f;
    float spreadDeg = 180.f;   // cone half-angle around the direction (180 = sphere)
    float upBias = 0.f;        // added to velocity.y
    float lifeMin = 0.3f, lifeMax = 0.6f;
    float sizeStart = 0.2f, sizeEnd = 0.f;
    Color colorStart{255, 255, 255, 255}, colorEnd{255, 255, 255, 0};
    float gravity = 0.f;
    float drag = 1.f;
    float jitter = 0.f;        // random start offset radius
    bool additive = true;
};

class Particles {
public:
    bool init(int maxParticles);
    void shutdown();
    void emit(const EmitterDef& e, Vector3 pos, Vector3 dir, Rng& rng, float countScale = 1.f);
    void update(float dt);
    void draw(Renderer& r);
    void clear() { alive_ = 0; }
    int alive() const { return alive_; }
    int capacity() const { return static_cast<int>(pool_.size()); }

private:
    struct P {
        Vector3 pos, vel;
        float life, maxLife, size0, size1, gravity, drag, rot, rotVel;
        Color c0, c1;
        bool additive;
    };
    std::vector<P> pool_;
    int alive_ = 0;
    Mesh quad_{};
    Material mat_{};
    std::vector<Matrix> addBatch_, alphaBatch_;
    int locRight_ = -1, locUp_ = -1;
    bool ready_ = false;
};

}  // namespace eng
