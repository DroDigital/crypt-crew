#pragma once
// Samples clips by time, crossfades between clips, fires events at set clip times and
// produces skinning matrices (GPU skinning) and joint world matrices (attachments).
#include <raylib.h>

#include <string>
#include <vector>

#include "engine/assets/model_asset.h"

namespace eng {

struct AnimEventDef {
    int clip = -1;
    float time = 0.f;   // seconds into the clip
    int id = 0;         // game-defined event id (hit, footstep, ...)
};

struct PlayParams {
    float speed = 1.f;
    bool loop = true;
    float fade = 0.12f;
    bool restart = false;  // restart even if this clip is already playing
};

class Animator {
public:
    static constexpr int kMaxEventsPerUpdate = 8;
    static constexpr int kMaxJoints = 64;

    void init(const ModelAsset* asset);
    void setEvents(const std::vector<AnimEventDef>* events) { events_ = events; }

    void play(int clip, const PlayParams& p = {});
    // Advances time; fills firedIds with event ids crossed this update. Returns how many fired.
    int update(float dt, int* firedIds = nullptr);

    int clip() const { return cur_.clip; }
    float time() const { return cur_.time; }
    float clipDuration() const;
    float normalized() const;  // 0..1 progress through the current clip
    bool finished() const { return !cur_.loop && cur_.time >= clipDuration(); }
    void setSpeed(float s) { cur_.speed = s; }

    // Evaluates the pose (call once per frame before drawing).
    void evaluate();
    const Matrix* skinMatrices() const { return skin_.data(); }
    int jointCount() const { return static_cast<int>(world_.size()); }
    // Joint transform in model space (multiply by the instance transform for world space).
    Matrix jointModel(int joint) const;
    const ModelAsset* asset() const { return asset_; }

private:
    struct Layer {
        int clip = -1;
        float time = 0.f;
        float speed = 1.f;
        bool loop = true;
    };
    void sampleInto(const Layer& layer, std::vector<Vector3>& t, std::vector<Quaternion>& r,
                    std::vector<Vector3>& s) const;
    static float wrapTime(const Layer& l, float duration);

    const ModelAsset* asset_ = nullptr;
    const std::vector<AnimEventDef>* events_ = nullptr;
    Layer cur_;
    Layer prev_;
    float fade_ = 0.f;
    float fadeDur_ = 0.f;
    std::vector<Vector3> t_, t2_, s_, s2_;
    std::vector<Quaternion> r_, r2_;
    std::vector<Matrix> world_, skin_;
};

}  // namespace eng
