#include "game/world/entities.h"

#include <cmath>

namespace game {

float Kinematic::lerpYaw(float a) const {
    float d = yaw - prevYaw;
    while (d > PI) d -= 2.f * PI;
    while (d < -PI) d += 2.f * PI;
    return prevYaw + d * a;
}

Vector3 Kinematic::forward() const { return Vector3{std::sin(yaw), 0.f, std::cos(yaw)}; }

}  // namespace game
