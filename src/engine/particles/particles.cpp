#include "engine/particles/particles.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <rlgl.h>

namespace eng {

bool Particles::init(int maxParticles) {
    pool_.resize(static_cast<size_t>(std::max(maxParticles, 16)));
    alive_ = 0;
    if (!IsWindowReady()) return false;
    // Unit quad in XY, centered.
    quad_ = Mesh{};
    quad_.vertexCount = 4;
    quad_.triangleCount = 2;
    quad_.vertices = static_cast<float*>(MemAlloc(4 * 3 * sizeof(float)));
    quad_.texcoords = static_cast<float*>(MemAlloc(4 * 2 * sizeof(float)));
    quad_.indices = static_cast<unsigned short*>(MemAlloc(6 * sizeof(unsigned short)));
    const float v[12] = {-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0.5f, 0.5f, 0, -0.5f, 0.5f, 0};
    const float t[8] = {0, 1, 1, 1, 1, 0, 0, 0};
    const unsigned short i[6] = {0, 1, 2, 0, 2, 3};
    std::copy(v, v + 12, quad_.vertices);
    std::copy(t, t + 8, quad_.texcoords);
    std::copy(i, i + 6, quad_.indices);
    UploadMesh(&quad_, false);
    mat_ = LoadMaterialDefault();
    ready_ = true;
    return true;
}

void Particles::shutdown() {
    if (!ready_) return;
    UnloadMesh(quad_);
    // Material's shader/texture are owned by the renderer; free only the maps array.
    MemFree(mat_.maps);
    mat_.maps = nullptr;
    ready_ = false;
}

void Particles::emit(const EmitterDef& e, Vector3 pos, Vector3 dir, Rng& rng, float countScale) {
    const int n = std::max(1, static_cast<int>(std::lround(static_cast<float>(e.count) * countScale)));
    Vector3 d = Vector3Length(dir) > 1e-4f ? Vector3Normalize(dir) : Vector3{0, 1, 0};
    // Basis around d.
    Vector3 t = std::fabs(d.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0};
    Vector3 u = Vector3Normalize(Vector3CrossProduct(d, t));
    Vector3 w = Vector3CrossProduct(d, u);
    for (int k = 0; k < n; ++k) {
        if (alive_ >= static_cast<int>(pool_.size())) return;  // global cap
        P& p = pool_[static_cast<size_t>(alive_++)];
        const float cosMax = std::cos(std::min(e.spreadDeg, 180.f) * DEG2RAD);
        const float cz = rng.uniform(cosMax, 1.f);
        const float sz = std::sqrt(std::max(0.f, 1.f - cz * cz));
        const float phi = rng.uniform(0.f, 2.f * PI);
        Vector3 dirK = Vector3Add(Vector3Scale(d, cz), Vector3Add(Vector3Scale(u, sz * std::cos(phi)), Vector3Scale(w, sz * std::sin(phi))));
        const float speed = rng.uniform(e.speedMin, e.speedMax);
        p.vel = Vector3Scale(dirK, speed);
        p.vel.y += e.upBias;
        p.pos = pos;
        if (e.jitter > 0.f) {
            p.pos.x += rng.uniform(-e.jitter, e.jitter);
            p.pos.y += rng.uniform(-e.jitter, e.jitter) * 0.5f;
            p.pos.z += rng.uniform(-e.jitter, e.jitter);
        }
        p.maxLife = p.life = rng.uniform(e.lifeMin, e.lifeMax);
        p.size0 = e.sizeStart * rng.uniform(0.75f, 1.25f);
        p.size1 = e.sizeEnd;
        p.gravity = e.gravity;
        p.drag = e.drag;
        p.rot = rng.uniform(0.f, 2.f * PI);
        p.rotVel = rng.uniform(-6.f, 6.f);
        p.c0 = e.colorStart;
        p.c1 = e.colorEnd;
        p.additive = e.additive;
    }
}

void Particles::update(float dt) {
    for (int i = 0; i < alive_;) {
        P& p = pool_[static_cast<size_t>(i)];
        p.life -= dt;
        if (p.life <= 0.f) {
            p = pool_[static_cast<size_t>(--alive_)];
            continue;
        }
        p.vel.y -= p.gravity * dt;
        const float k = std::max(0.f, 1.f - p.drag * dt);
        p.vel = Vector3Scale(p.vel, k);
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        if (p.pos.y < 0.03f && p.gravity > 0.f) {
            p.pos.y = 0.03f;
            p.vel.y = -p.vel.y * 0.3f;
            p.vel.x *= 0.6f;
            p.vel.z *= 0.6f;
        }
        p.rot += p.rotVel * dt;
        ++i;
    }
}

void Particles::draw(Renderer& r) {
    if (!ready_ || alive_ == 0) return;
    const Camera3D& cam = r.camera();
    const Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
    Vector3 right{view.m0, view.m4, view.m8};
    Vector3 up{view.m1, view.m5, view.m9};
    Shader sh = r.particleShader();
    if (locRight_ < 0) {
        locRight_ = GetShaderLocation(sh, "camRight");
        locUp_ = GetShaderLocation(sh, "camUp");
    }
    SetShaderValue(sh, locRight_, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, locUp_, &up, SHADER_UNIFORM_VEC3);
    mat_.shader = sh;
    mat_.maps[MATERIAL_MAP_DIFFUSE].texture = r.softCircle();

    addBatch_.clear();
    alphaBatch_.clear();
    for (int i = 0; i < alive_; ++i) {
        const P& p = pool_[static_cast<size_t>(i)];
        const float t = 1.f - p.life / p.maxLife;
        const float size = p.size0 + (p.size1 - p.size0) * t;
        Matrix m{};
        // Columns (see particle.vs): [0]=pos,size [1]=rgba [2].x=rotation. MatrixToFloatV is column-major (m0..m3 = col 0).
        m.m0 = p.pos.x; m.m1 = p.pos.y; m.m2 = p.pos.z; m.m3 = size;
        m.m4 = (p.c0.r + (p.c1.r - p.c0.r) * t) / 255.f;
        m.m5 = (p.c0.g + (p.c1.g - p.c0.g) * t) / 255.f;
        m.m6 = (p.c0.b + (p.c1.b - p.c0.b) * t) / 255.f;
        m.m7 = (p.c0.a + (p.c1.a - p.c0.a) * t) / 255.f;
        m.m8 = p.rot;
        (p.additive ? addBatch_ : alphaBatch_).push_back(m);
    }
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    if (!alphaBatch_.empty()) {
        BeginBlendMode(BLEND_ALPHA);
        DrawMeshInstanced(quad_, mat_, alphaBatch_.data(), static_cast<int>(alphaBatch_.size()));
        EndBlendMode();
    }
    if (!addBatch_.empty()) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawMeshInstanced(quad_, mat_, addBatch_.data(), static_cast<int>(addBatch_.size()));
        EndBlendMode();
    }
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

}  // namespace eng
