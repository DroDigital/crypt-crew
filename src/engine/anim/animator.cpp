#include "engine/anim/animator.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace eng {
namespace {

// Index of the last key with time <= t (binary search).
size_t keyIndex(const std::vector<float>& times, float t) {
    auto it = std::upper_bound(times.begin(), times.end(), t);
    if (it == times.begin()) return 0;
    return static_cast<size_t>(it - times.begin()) - 1;
}

void sampleTrack(const AnimTrack& tr, float t, float* out, int comps) {
    const size_t n = tr.times.size();
    if (n == 0) return;
    if (n == 1 || t <= tr.times.front()) {
        for (int c = 0; c < comps; ++c) out[c] = tr.values[static_cast<size_t>(c)];
        return;
    }
    if (t >= tr.times.back()) {
        for (int c = 0; c < comps; ++c) out[c] = tr.values[(n - 1) * static_cast<size_t>(comps) + static_cast<size_t>(c)];
        return;
    }
    const size_t k = keyIndex(tr.times, t);
    const float* a = &tr.values[k * static_cast<size_t>(comps)];
    const float* b = &tr.values[(k + 1) * static_cast<size_t>(comps)];
    if (tr.step) {
        for (int c = 0; c < comps; ++c) out[c] = a[c];
        return;
    }
    const float span = tr.times[k + 1] - tr.times[k];
    const float f = span > 0.f ? (t - tr.times[k]) / span : 0.f;
    if (comps == 4) {
        Quaternion q = QuaternionSlerp(Quaternion{a[0], a[1], a[2], a[3]}, Quaternion{b[0], b[1], b[2], b[3]}, f);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
    } else {
        for (int c = 0; c < comps; ++c) out[c] = a[c] + (b[c] - a[c]) * f;
    }
}

}  // namespace

void Animator::init(const ModelAsset* asset) {
    asset_ = asset;
    const size_t n = asset ? static_cast<size_t>(asset->skeleton.jointCount()) : 0;
    t_.resize(n); t2_.resize(n); s_.resize(n); s2_.resize(n); r_.resize(n); r2_.resize(n);
    world_.assign(n, MatrixIdentity());
    skin_.assign(std::max<size_t>(n, 1), MatrixIdentity());
    cur_ = Layer{};
    prev_ = Layer{};
    fade_ = fadeDur_ = 0.f;
}

float Animator::clipDuration() const {
    if (!asset_ || cur_.clip < 0) return 0.f;
    return asset_->clips[static_cast<size_t>(cur_.clip)].duration;
}

float Animator::normalized() const {
    const float d = clipDuration();
    if (d <= 0.f) return 1.f;
    return std::clamp(wrapTime(cur_, d) / d, 0.f, 1.f);
}

float Animator::wrapTime(const Layer& l, float duration) {
    if (duration <= 0.f) return 0.f;
    if (l.loop) return std::fmod(l.time, duration);
    return std::min(l.time, duration);
}

void Animator::play(int clip, const PlayParams& p) {
    if (!asset_ || clip < 0 || clip >= static_cast<int>(asset_->clips.size())) return;
    if (clip == cur_.clip && !p.restart) {
        cur_.speed = p.speed;
        cur_.loop = p.loop;
        return;
    }
    if (cur_.clip >= 0 && p.fade > 0.f) {
        prev_ = cur_;
        fade_ = 0.f;
        fadeDur_ = p.fade;
    } else {
        fadeDur_ = 0.f;
    }
    cur_ = Layer{clip, 0.f, p.speed, p.loop};
}

int Animator::update(float dt, int* firedIds) {
    if (!asset_ || cur_.clip < 0) return 0;
    int fired = 0;
    const float dur = clipDuration();
    const float before = cur_.time;
    cur_.time += dt * cur_.speed;
    if (events_ && dur > 0.f) {
        // Fire events whose time lies in (before, after], handling loop wrap.
        const float a = before;
        const float b = cur_.time;
        for (const AnimEventDef& e : *events_) {
            if (e.clip != cur_.clip) continue;
            bool hit = false;
            if (cur_.loop) {
                const float la = std::fmod(a, dur);
                const float span = b - a;
                const float lb = la + span;
                hit = (e.time > la && e.time <= lb) || (e.time + dur > la && e.time + dur <= lb) ||
                      (a == 0.f && e.time == 0.f);
            } else {
                hit = (e.time > a || (a == 0.f && e.time == 0.f)) && e.time <= b;
            }
            if (hit && fired < kMaxEventsPerUpdate) {
                if (firedIds) firedIds[fired] = e.id;
                ++fired;
            }
        }
    }
    if (fadeDur_ > 0.f) {
        prev_.time += dt * prev_.speed;
        fade_ += dt;
        if (fade_ >= fadeDur_) fadeDur_ = 0.f;
    }
    return fired;
}

void Animator::sampleInto(const Layer& layer, std::vector<Vector3>& t, std::vector<Quaternion>& r,
                          std::vector<Vector3>& s) const {
    const Skeleton& sk = asset_->skeleton;
    t = sk.restT;
    r = sk.restR;
    s = sk.restS;
    if (layer.clip < 0) return;
    const AnimClip& clip = asset_->clips[static_cast<size_t>(layer.clip)];
    const float time = wrapTime(layer, clip.duration);
    for (const AnimTrack& tr : clip.tracks) {
        const size_t j = static_cast<size_t>(tr.joint);
        switch (tr.path) {
            case AnimTrack::Path::Translation: sampleTrack(tr, time, &t[j].x, 3); break;
            case AnimTrack::Path::Rotation: sampleTrack(tr, time, &r[j].x, 4); break;
            case AnimTrack::Path::Scale: sampleTrack(tr, time, &s[j].x, 3); break;
        }
    }
}

void Animator::evaluate() {
    if (!asset_ || !asset_->skinned()) return;
    sampleInto(cur_, t_, r_, s_);
    if (fadeDur_ > 0.f && prev_.clip >= 0) {
        sampleInto(prev_, t2_, r2_, s2_);
        const float w = std::clamp(fade_ / fadeDur_, 0.f, 1.f);
        for (size_t j = 0; j < t_.size(); ++j) {
            t_[j] = Vector3Lerp(t2_[j], t_[j], w);
            s_[j] = Vector3Lerp(s2_[j], s_[j], w);
            r_[j] = QuaternionSlerp(r2_[j], r_[j], w);
        }
    }
    const Skeleton& sk = asset_->skeleton;
    for (size_t j = 0; j < world_.size(); ++j) {
        const Matrix local = MatrixMultiply(
            MatrixMultiply(MatrixScale(s_[j].x, s_[j].y, s_[j].z), QuaternionToMatrix(QuaternionNormalize(r_[j]))),
            MatrixTranslate(t_[j].x, t_[j].y, t_[j].z));
        const int p = sk.parent[j];
        world_[j] = MatrixMultiply(local, p >= 0 ? world_[static_cast<size_t>(p)] : sk.rootParent[j]);
        skin_[j] = MatrixMultiply(sk.inverseBind[j], world_[j]);
    }
}

Matrix Animator::jointModel(int joint) const {
    if (joint < 0 || joint >= static_cast<int>(world_.size())) return MatrixIdentity();
    return world_[static_cast<size_t>(joint)];
}

}  // namespace eng
