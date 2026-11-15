#include "game/world/camera_rig.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace game {
namespace {

Vector3 centerOf(const std::vector<Vector3>& pts, Vector3 fallback) {
    if (pts.empty()) return fallback;
    Vector3 mn = pts[0], mx = pts[0];
    for (const auto& p : pts) {
        mn = Vector3Min(mn, p);
        mx = Vector3Max(mx, p);
    }
    return Vector3Scale(Vector3Add(mn, mx), 0.5f);
}

}  // namespace

float CameraRig::fitDistance(const std::vector<Vector3>& pts, Vector3 c, float aspect) const {
    float dx = 0.f, dz = 0.f;
    for (const auto& p : pts) {
        dx = std::max(dx, std::fabs(p.x - c.x));
        dz = std::max(dz, std::fabs(p.z - c.z));
    }
    dx += s_.fitMargin;
    dz += s_.fitMargin;
    const float t = std::tan(s_.fovy * 0.5f * DEG2RAD);
    const float pitch = s_.pitchDeg * DEG2RAD;
    const float needX = dx / (t * aspect);
    const float needZ = dz * std::sin(pitch) / t;
    return std::clamp(std::max(needX, needZ), s_.minDistance, s_.maxDistance);
}

Vector3 CameraRig::clampFocus(Vector3 f) const {
    if (!hasBounds_) return f;
    auto clampAxis = [](float v, float lo, float hi) { return lo > hi ? (lo + hi) * 0.5f : std::clamp(v, lo, hi); };
    f.x = clampAxis(f.x, boundsMin_.x + 7.f, boundsMax_.x - 7.f);
    f.z = clampAxis(f.z, boundsMin_.z + 4.f, boundsMax_.z - 7.f);
    return f;
}

void CameraRig::snap(const std::vector<Vector3>& pts, float aspect) {
    focus_ = clampFocus(centerOf(pts, focus_));
    distance_ = fitDistance(pts, focus_, aspect);
}

void CameraRig::update(const std::vector<Vector3>& pts, float aspect, float dt) {
    const Vector3 c = clampFocus(centerOf(pts, focus_));
    const float k = 1.f - std::exp(-s_.followRate * dt);
    focus_ = Vector3Lerp(focus_, c, k);
    const float want = fitDistance(pts, focus_, aspect);
    distance_ += (want - distance_) * (1.f - std::exp(-s_.zoomRate * dt));
    trauma_ = std::max(0.f, trauma_ - s_.shakeDecay * dt * 0.5f);
}

void CameraRig::addTrauma(float t) { trauma_ = std::min(1.f, trauma_ + t); }

Camera3D CameraRig::camera(float time) const {
    const float pitch = s_.pitchDeg * DEG2RAD, yaw = s_.yawDeg * DEG2RAD;
    Vector3 target{focus_.x, focus_.y + 1.0f, focus_.z};
    const Vector3 back{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    Camera3D cam{};
    cam.target = target;
    cam.position = Vector3Add(target, Vector3Scale(back, distance_));
    cam.up = Vector3{0, 1, 0};
    cam.fovy = s_.fovy;
    cam.projection = CAMERA_PERSPECTIVE;
    if (trauma_ > 0.f) {
        const float s = trauma_ * trauma_ * s_.shakeMaxOffset;
        const Vector3 off{s * std::sin(time * 47.f), s * std::sin(time * 59.f + 1.3f) * 0.6f, s * std::sin(time * 53.f + 2.1f)};
        cam.position = Vector3Add(cam.position, off);
        cam.target = Vector3Add(cam.target, Vector3Scale(off, 0.6f));
    }
    return cam;
}

Vector2 CameraRig::leashHalfExtents(float aspect) const {
    const float t = std::tan(s_.fovy * 0.5f * DEG2RAD);
    const float pitch = s_.pitchDeg * DEG2RAD;
    const float hx = s_.maxDistance * t * aspect - s_.leashMargin;
    const float hz = s_.maxDistance * t / std::sin(pitch) - s_.leashMargin;
    return Vector2{std::max(hx, 2.f), std::max(hz, 2.f)};
}

Vector3 CameraRig::clampToLeash(Vector3 p, float aspect) const {
    const Vector2 h = leashHalfExtents(aspect);
    p.x = std::clamp(p.x, focus_.x - h.x, focus_.x + h.x);
    p.z = std::clamp(p.z, focus_.z - h.y, focus_.z + h.y);
    return p;
}

}  // namespace game
