#include "engine/render/renderer.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <rlgl.h>
#include <string>

#include "engine/core/log.h"
#include "engine/core/paths.h"

namespace eng {
namespace {

Image makeRadialImage(int size, float inner, float outer, bool ring) {
    Image img = GenImageColor(size, size, BLANK);
    auto* px = static_cast<Color*>(img.data);
    const float c = (static_cast<float>(size) - 1.f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float d = std::sqrt((static_cast<float>(x) - c) * (static_cast<float>(x) - c) +
                                      (static_cast<float>(y) - c) * (static_cast<float>(y) - c)) / c;
            float a;
            if (ring) {
                const float mid = (inner + outer) * 0.5f, half = (outer - inner) * 0.5f;
                a = std::clamp(1.f - std::fabs(d - mid) / half, 0.f, 1.f);
                a = std::sqrt(a);
                if (d < inner) a = std::max(a, 0.18f);  // faint fill inside the ring
            } else {
                a = std::clamp((outer - d) / std::max(outer - inner, 0.001f), 0.f, 1.f);
            }
            px[y * size + x] = Color{255, 255, 255, static_cast<unsigned char>(a * 255.f)};
        }
    }
    return img;
}

Texture2D texFromImage(Image img) {
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    return t;
}

std::string readText(const std::string& rel) {
    char* text = LoadFileText(resPath(rel).c_str());
    if (!text) {
        log::error("missing shader %s", rel.c_str());
        return {};
    }
    std::string s(text);
    UnloadFileText(text);
    return s;
}

std::string withDefine(const std::string& src, const char* define) {
    // Insert after the #version line.
    const size_t nl = src.find('\n');
    if (nl == std::string::npos) return src;
    return src.substr(0, nl + 1) + "#define " + define + "\n" + src.substr(nl + 1);
}

}  // namespace

bool Renderer::loadLit(LitShader& s, bool skinned) {
    std::string vs = readText("assets/shaders/lit.vs");
    std::string fs = readText("assets/shaders/lit.fs");
    if (vs.empty() || fs.empty()) return false;
    if (skinned) vs = withDefine(vs, "SKINNED");
    s.shader = LoadShaderFromMemory(vs.c_str(), fs.c_str());
    if (!IsShaderValid(s.shader)) {
        log::error("lit shader failed to compile (skinned=%d)", skinned ? 1 : 0);
        return false;
    }
    Shader& sh = s.shader;
    s.locAmbient = GetShaderLocation(sh, "ambientColor");
    s.locSunDir = GetShaderLocation(sh, "sunDir");
    s.locSunColor = GetShaderLocation(sh, "sunColor");
    s.locLightCount = GetShaderLocation(sh, "lightCount");
    s.locLightPos = GetShaderLocation(sh, "lightPos");
    s.locLightColor = GetShaderLocation(sh, "lightColor");
    s.locLightRadius = GetShaderLocation(sh, "lightRadius");
    s.locViewPos = GetShaderLocation(sh, "viewPos");
    s.locFogColor = GetShaderLocation(sh, "fogColor");
    s.locFogRange = GetShaderLocation(sh, "fogRange");
    s.locTint = GetShaderLocation(sh, "tint");
    s.locFlash = GetShaderLocation(sh, "flash");
    s.locEmissive = GetShaderLocation(sh, "emissive");
    return true;
}

bool Renderer::init() {
    glTargetsInit();
    bool ok = loadLit(lit_, false) && loadLit(litSkinned_, true);
    particle_ = LoadShader(resPath("assets/shaders/particle.vs").c_str(), resPath("assets/shaders/particle.fs").c_str());
    bright_ = LoadShader(nullptr, resPath("assets/shaders/bright.fs").c_str());
    blur_ = LoadShader(nullptr, resPath("assets/shaders/blur.fs").c_str());
    composite_ = LoadShader(nullptr, resPath("assets/shaders/composite.fs").c_str());
    for (Shader* s : {&particle_, &bright_, &blur_, &composite_})
        if (!IsShaderValid(*s)) {
            log::error("post/particle shader failed to load");
            ok = false;
        }
    locThreshold_ = GetShaderLocation(bright_, "threshold");
    locTexelDir_ = GetShaderLocation(blur_, "texelDir");
    locBloomTex_ = GetShaderLocation(composite_, "bloomTex");
    locBloomStrength_ = GetShaderLocation(composite_, "bloomStrength");
    locExposure_ = GetShaderLocation(composite_, "exposure");
    locVignette_ = GetShaderLocation(composite_, "vignette");
    locFade_ = GetShaderLocation(composite_, "fade");

    texSoft_ = texFromImage(makeRadialImage(64, 0.f, 1.f, false));
    texDisc_ = texFromImage(makeRadialImage(128, 0.9f, 1.f, false));
    texRing_ = texFromImage(makeRadialImage(128, 0.82f, 1.f, true));
    ready_ = ok;
    return ok;
}

void Renderer::shutdown() {
    destroyTargets();
    for (Shader s : {lit_.shader, litSkinned_.shader, particle_, bright_, blur_, composite_})
        if (s.id) UnloadShader(s);
    for (Texture2D t : {texSoft_, texDisc_, texRing_})
        if (t.id) UnloadTexture(t);
    ready_ = false;
}

void Renderer::destroyTargets() {
    destroyMsaaTarget(msaa_);
    destroyHdrTarget(scene_);
    destroyHdrTarget(bloomA_);
    destroyHdrTarget(bloomB_);
    targetW_ = targetH_ = 0;
}

void Renderer::ensureTargets() {
    const int w = std::max(GetRenderWidth(), 1), h = std::max(GetRenderHeight(), 1);
    if (w == targetW_ && h == targetH_) return;
    destroyTargets();
    targetW_ = w;
    targetH_ = h;
    const int samples = std::min(4, maxMsaaSamples());
    msaa_ = createMsaaTarget(w, h, samples);
    scene_ = createHdrTarget(w, h, msaa_.fbo == 0);  // own depth only if not resolving from MSAA
    bloomA_ = createHdrTarget(std::max(w / 2, 1), std::max(h / 2, 1), false);
    bloomB_ = createHdrTarget(std::max(w / 2, 1), std::max(h / 2, 1), false);
}

void Renderer::setFrameUniforms(LitShader& s) {
    const Shader& sh = s.shader;
    const Lighting& L = lighting_;
    Vector3 sd = Vector3Normalize(L.sunDir);
    SetShaderValue(sh, s.locAmbient, &L.ambient, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, s.locSunDir, &sd, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, s.locSunColor, &L.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, s.locViewPos, &cam_.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, s.locFogColor, &L.fogColor, SHADER_UNIFORM_VEC3);
    Vector2 fr{L.fogStart, L.fogEnd};
    SetShaderValue(sh, s.locFogRange, &fr, SHADER_UNIFORM_VEC2);
}

void Renderer::setLights(LitShader& s, Vector3 center) {
    // Pick the 8 lights that matter most for this draw (closest relative to their radius).
    struct Cand {
        float score;
        int idx;
    };
    Cand cands[64];
    int n = 0;
    for (int i = 0; i < static_cast<int>(lights_.size()) && n < 64; ++i) {
        const float d = Vector3Distance(center, lights_[static_cast<size_t>(i)].pos);
        cands[n++] = Cand{d - lights_[static_cast<size_t>(i)].radius, i};
    }
    const int count = std::min(n, 8);
    std::partial_sort(cands, cands + count, cands + n, [](const Cand& a, const Cand& b) { return a.score < b.score; });
    Vector3 pos[8], col[8];
    float rad[8];
    for (int i = 0; i < count; ++i) {
        const PointLight& l = lights_[static_cast<size_t>(cands[i].idx)];
        pos[i] = l.pos;
        col[i] = l.color;
        rad[i] = l.radius;
    }
    SetShaderValue(s.shader, s.locLightCount, &count, SHADER_UNIFORM_INT);
    if (count > 0) {
        SetShaderValueV(s.shader, s.locLightPos, pos, SHADER_UNIFORM_VEC3, count);
        SetShaderValueV(s.shader, s.locLightColor, col, SHADER_UNIFORM_VEC3, count);
        SetShaderValueV(s.shader, s.locLightRadius, rad, SHADER_UNIFORM_FLOAT, count);
    }
}

void Renderer::beginScene(const Camera3D& cam, const Lighting& lighting, const std::vector<PointLight>& lights) {
    ensureTargets();
    cam_ = cam;
    lighting_ = lighting;
    lights_ = lights;
    drawCalls_ = 0;
    decals_.clear();
    shadows_.clear();
    setFrameUniforms(lit_);
    setFrameUniforms(litSkinned_);

    rlDrawRenderBatchActive();
    savedModelview_ = rlGetMatrixModelview();
    rlEnableFramebuffer(msaa_.fbo ? msaa_.fbo : scene_.fbo);
    rlViewport(0, 0, targetW_, targetH_);
    const Vector3 fc = lighting.fogColor;
    rlClearColor(static_cast<unsigned char>(std::clamp(fc.x, 0.f, 1.f) * 255.f),
                 static_cast<unsigned char>(std::clamp(fc.y, 0.f, 1.f) * 255.f),
                 static_cast<unsigned char>(std::clamp(fc.z, 0.f, 1.f) * 255.f), 255);
    rlClearScreenBuffers();

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    const double aspect = static_cast<double>(targetW_) / static_cast<double>(targetH_);
    const double nearP = 0.1, farP = 300.0;
    const double top = nearP * std::tan(cam.fovy * 0.5 * DEG2RAD);
    const double right = top * aspect;
    rlFrustum(-right, right, -top, top, nearP, farP);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
}

void Renderer::beginPreview(RenderTexture2D& target, const Camera3D& cam, const Lighting& lighting,
                            const std::vector<PointLight>& lights, Color clear) {
    cam_ = cam;
    lighting_ = lighting;
    lights_ = lights;
    setFrameUniforms(lit_);
    setFrameUniforms(litSkinned_);
    BeginTextureMode(target);
    ClearBackground(clear);
    BeginMode3D(cam);
}

void Renderer::endPreview() {
    EndMode3D();
    EndTextureMode();
}

void Renderer::drawModel(const ModelAsset& asset, const Matrix& transform, const DrawParams& p) {
    const Model& m = asset.model;
    if (m.meshCount == 0) return;
    const Vector3 center{transform.m12, transform.m13 + 1.f, transform.m14};
    bool litReady = false, skinReady = false;
    const Matrix world = MatrixMultiply(m.transform, transform);
    const Vector4 tint{p.tint.r / 255.f, p.tint.g / 255.f, p.tint.b / 255.f, p.tint.a / 255.f};
    for (int i = 0; i < m.meshCount; ++i) {
        if (i < 64 && (p.hiddenMeshes >> i) & 1ULL) continue;
        const bool skinned = p.skin && asset.meshSkinned[static_cast<size_t>(i)];
        LitShader& ls = skinned ? litSkinned_ : lit_;
        bool& prepared = skinned ? skinReady : litReady;
        if (!prepared) {
            setLights(ls, center);
            SetShaderValue(ls.shader, ls.locTint, &tint, SHADER_UNIFORM_VEC4);
            SetShaderValue(ls.shader, ls.locFlash, &p.flash, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ls.shader, ls.locEmissive, &p.emissive, SHADER_UNIFORM_VEC3);
            if (skinned) {
                rlEnableShader(ls.shader.id);
                rlSetUniformMatrices(ls.shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS], p.skin,
                                     std::min(p.jointCount, 64));
            }
            prepared = true;
        }
        Material mat = m.materials[m.meshMaterial[i]];
        mat.shader = ls.shader;
        DrawMesh(m.meshes[i], mat, world);
        ++drawCalls_;
    }
}

void Renderer::shadow(Vector3 pos, float radius, float alpha) {
    shadows_.push_back(Decal{pos, radius, Color{0, 0, 0, static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f)},
                             DecalShape::Shadow, 0.f});
}

void Renderer::decal(Vector3 pos, float radius, Color color, DecalShape shape, float rotation) {
    decals_.push_back(Decal{pos, radius, color, shape, rotation});
}

void Renderer::flushDecals() {
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    auto drawList = [&](const std::vector<Decal>& list, float lift) {
        for (const Decal& d : list) {
            Texture2D tex = d.shape == DecalShape::Ring ? texRing_ : d.shape == DecalShape::Disc ? texDisc_ : texSoft_;
            rlSetTexture(tex.id);
            rlBegin(RL_QUADS);
            rlColor4ub(d.color.r, d.color.g, d.color.b, d.color.a);
            const float c = std::cos(d.rotation) * d.radius, s = std::sin(d.rotation) * d.radius;
            const float y = d.pos.y + lift;
            // Corners (-1,-1) (-1,1) (1,1) (1,-1) rotated around Y.
            rlTexCoord2f(0, 0); rlVertex3f(d.pos.x - c + s, y, d.pos.z - s - c);
            rlTexCoord2f(0, 1); rlVertex3f(d.pos.x - c - s, y, d.pos.z - s + c);
            rlTexCoord2f(1, 1); rlVertex3f(d.pos.x + c - s, y, d.pos.z + s + c);
            rlTexCoord2f(1, 0); rlVertex3f(d.pos.x + c + s, y, d.pos.z + s - c);
            rlEnd();
            rlSetTexture(0);
        }
        rlDrawRenderBatchActive();
    };
    drawList(shadows_, 0.03f);
    drawList(decals_, 0.05f);
    shadows_.clear();
    decals_.clear();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void Renderer::endScene() {
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlMultMatrixf(MatrixToFloat(savedModelview_));
    rlDisableDepthTest();
    rlDisableFramebuffer();
    if (msaa_.fbo) resolveMsaa(msaa_, scene_);
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
}

void Renderer::pass(Shader sh, Texture2D src, const HdrTarget& dst) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(dst.fbo);
    rlViewport(0, 0, dst.width, dst.height);
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0, dst.width, dst.height, 0, 0, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();
    BeginShaderMode(sh);
    DrawTexturePro(src, Rectangle{0, 0, static_cast<float>(src.width), -static_cast<float>(src.height)},
                   Rectangle{0, 0, static_cast<float>(dst.width), static_cast<float>(dst.height)}, Vector2{0, 0}, 0.f,
                   WHITE);
    EndShaderMode();
    // rlgl has one matrix stack shared by both modes: pop in reverse push order.
    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlDisableFramebuffer();
}

void Renderer::present(float fadeToBlack) {
    if (!scene_.fbo) return;
    rlDisableColorBlend();
    SetShaderValue(bright_, locThreshold_, &lighting_.bloomThreshold, SHADER_UNIFORM_FLOAT);
    pass(bright_, scene_.color, bloomA_);
    for (int i = 0; i < 2; ++i) {
        Vector2 h{1.f / static_cast<float>(bloomA_.width), 0.f};
        SetShaderValue(blur_, locTexelDir_, &h, SHADER_UNIFORM_VEC2);
        pass(blur_, bloomA_.color, bloomB_);
        Vector2 v{0.f, 1.f / static_cast<float>(bloomA_.height)};
        SetShaderValue(blur_, locTexelDir_, &v, SHADER_UNIFORM_VEC2);
        pass(blur_, bloomB_.color, bloomA_);
    }
    rlEnableColorBlend();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());

    BeginShaderMode(composite_);
    SetShaderValueTexture(composite_, locBloomTex_, bloomA_.color);
    SetShaderValue(composite_, locBloomStrength_, &lighting_.bloomStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(composite_, locExposure_, &lighting_.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(composite_, locVignette_, &lighting_.vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(composite_, locFade_, &fadeToBlack, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(scene_.color,
                   Rectangle{0, 0, static_cast<float>(scene_.width), -static_cast<float>(scene_.height)},
                   Rectangle{0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
                   Vector2{0, 0}, 0.f, WHITE);
    EndShaderMode();
}

Vector2 Renderer::worldToScreen(Vector3 p) const {
    return GetWorldToScreenEx(p, cam_, GetScreenWidth(), GetScreenHeight());
}

Ray Renderer::screenRay(Vector2 screen) const {
    return GetScreenToWorldRayEx(screen, cam_, GetScreenWidth(), GetScreenHeight());
}

}  // namespace eng
