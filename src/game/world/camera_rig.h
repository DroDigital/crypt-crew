#pragma once
// Shared perspective camera (~55 degree pitch) that follows the center of living players and
// zooms out to fit everyone, clamped between min and max distance. Trauma-based shake.
#include <raylib.h>

#include <vector>

#include "engine/core/rng.h"

namespace game {

struct CameraSettings {
    float pitchDeg = 55.f, yawDeg = 0.f, fovy = 40.f;
    float minDistance = 18.f, maxDistance = 32.f, fitMargin = 4.f;
    float followRate = 6.f, zoomRate = 3.f, leashMargin = 1.2f;
    float shakeDecay = 3.5f, shakeMaxOffset = 0.6f;
};

class CameraRig {
public:
    void configure(const CameraSettings& s) { s_ = s; }
    void snap(const std::vector<Vector3>& points, float aspect);
    void update(const std::vector<Vector3>& points, float aspect, float dt);
    void addTrauma(float t);
    // Keep the focus inside the room (minus margins) so the view doesn't drift over the void.
    void setBounds(Vector3 mn, Vector3 mx) {
        boundsMin_ = mn;
        boundsMax_ = mx;
        hasBounds_ = true;
    }
    Camera3D camera(float time) const;  // with shake applied
    Vector3 focus() const { return focus_; }
    float distance() const { return distance_; }
    // Half extents (x, z) of the ground area that is guaranteed visible at max zoom, minus margin.
    Vector2 leashHalfExtents(float aspect) const;
    // Clamp a desired position so it stays inside the leash around the current focus.
    Vector3 clampToLeash(Vector3 p, float aspect) const;
    const CameraSettings& settings() const { return s_; }

private:
    float fitDistance(const std::vector<Vector3>& pts, Vector3 center, float aspect) const;
    CameraSettings s_;
    Vector3 focus_{0, 0, 0};
    float distance_ = 20.f;
    float trauma_ = 0.f;
    Vector3 boundsMin_{}, boundsMax_{};
    bool hasBounds_ = false;
    Vector3 clampFocus(Vector3 f) const;
};

}  // namespace game
