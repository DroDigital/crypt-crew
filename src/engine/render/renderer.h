#pragma once
// Forward renderer: one lit shader (ambient + sun + 8 nearest point lights + fog, hit flash
// and tint as uniforms), GPU skinning, blob shadows, ground decals and a post pass
// (bloom, tonemap, vignette) over a 4x MSAA HDR target.
#include <raylib.h>

#include <cstdint>
#include <vector>

#include "engine/assets/model_asset.h"
#include "engine/render/gl_targets.h"

namespace eng {

struct PointLight {
    Vector3 pos{};
    Vector3 color{1, 1, 1};  // may exceed 1 (HDR)
    float radius = 8.f;
};

struct Lighting {
    Vector3 ambient{0.35f, 0.38f, 0.45f};
    Vector3 sunDir{-0.4f, -1.f, -0.3f};
    Vector3 sunColor{0.5f, 0.5f, 0.55f};
    Vector3 fogColor{0.05f, 0.06f, 0.09f};
    float fogStart = 30.f;
    float fogEnd = 80.f;
    float exposure = 1.f;
    float bloomThreshold = 0.95f;
    float bloomStrength = 0.6f;
    float vignette = 0.35f;
};

struct DrawParams {
    const Matrix* skin = nullptr;  // skinning matrices (Animator::skinMatrices)
    int jointCount = 0;
    Color tint = WHITE;
    float flash = 0.f;
    Vector3 emissive{0, 0, 0};
    uint64_t hiddenMeshes = 0;     // bit i hides mesh i
};

enum class DecalShape { Disc, Ring, Shadow };

class Renderer {
public:
    bool init();
    void shutdown();

    void beginScene(const Camera3D& cam, const Lighting& lighting, const std::vector<PointLight>& lights);
    void drawModel(const ModelAsset& asset, const Matrix& transform, const DrawParams& p = {});
    void shadow(Vector3 pos, float radius, float alpha = 0.5f);
    void decal(Vector3 pos, float radius, Color color, DecalShape shape, float rotation = 0.f);
    void flushDecals();  // draw queued shadows/decals (call after opaque, before particles)
    void endScene();
    void present(float fadeToBlack = 0.f);  // post-process into the backbuffer (inside BeginDrawing)

    const Camera3D& camera() const { return cam_; }
    Vector2 worldToScreen(Vector3 p) const;   // logical screen coordinates
    Ray screenRay(Vector2 screen) const;
    Shader particleShader() const { return particle_; }
    Texture2D softCircle() const { return texSoft_; }
    int drawCalls() const { return drawCalls_; }
    int msaaSamples() const { return msaa_.samples; }
    bool ready() const { return ready_; }

private:
    struct LitShader {
        Shader shader{};
        int locAmbient = -1, locSunDir = -1, locSunColor = -1, locLightCount = -1, locLightPos = -1,
            locLightColor = -1, locLightRadius = -1, locViewPos = -1, locFogColor = -1, locFogRange = -1,
            locTint = -1, locFlash = -1, locEmissive = -1;
    };
    struct Decal {
        Vector3 pos;
        float radius;
        Color color;
        DecalShape shape;
        float rotation;
    };
    bool loadLit(LitShader& s, bool skinned);
    void setFrameUniforms(LitShader& s);
    void setLights(LitShader& s, Vector3 center);
    void ensureTargets();
    void destroyTargets();
    void pass(Shader sh, Texture2D src, const HdrTarget& dst);

    bool ready_ = false;
    LitShader lit_, litSkinned_;
    Shader particle_{}, bright_{}, blur_{}, composite_{};
    int locThreshold_ = -1, locTexelDir_ = -1, locBloomTex_ = -1, locBloomStrength_ = -1, locExposure_ = -1,
        locVignette_ = -1, locFade_ = -1;
    Texture2D texSoft_{}, texDisc_{}, texRing_{};
    MsaaTarget msaa_;
    HdrTarget scene_, bloomA_, bloomB_;
    int targetW_ = 0, targetH_ = 0;
    Camera3D cam_{};
    Lighting lighting_;
    std::vector<PointLight> lights_;
    std::vector<Decal> decals_, shadows_;
    Matrix savedModelview_{};
    int drawCalls_ = 0;
};

}  // namespace eng
