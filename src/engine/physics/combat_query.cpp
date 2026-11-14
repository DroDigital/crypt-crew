#include "engine/physics/combat_query.h"

#include <cmath>
#include <raymath.h>

namespace eng {
namespace {

bool verticalOverlap(float aMin, float aMax, float bMin, float bMax) { return aMin <= bMax && bMin <= aMax; }

float dist2D(Vector3 a, Vector3 b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

bool overlaps(const Hitbox& hb, const Hurtbox& hurt) {
    switch (hb.shape) {
        case HitShape::Sphere: {
            // Closest point on the cylinder axis segment to the sphere center.
            const float y = std::clamp(hb.origin.y, hurt.feet.y, hurt.feet.y + hurt.height);
            const float d2 = dist2D(hb.origin, hurt.feet);
            const float dy = hb.origin.y - y;
            const float horiz = std::max(d2 - hurt.radius, 0.f);
            return horiz * horiz + dy * dy <= hb.radius * hb.radius;
        }
        case HitShape::Capsule: {
            // Segment (2D projection) vs circle, with height check on the segment's y range.
            const Vector2 a{hb.origin.x, hb.origin.z}, b{hb.end.x, hb.end.z}, p{hurt.feet.x, hurt.feet.z};
            const Vector2 ab = Vector2Subtract(b, a);
            const float len2 = Vector2LengthSqr(ab);
            const float t = len2 > 0.f ? std::clamp(Vector2DotProduct(Vector2Subtract(p, a), ab) / len2, 0.f, 1.f) : 0.f;
            const Vector2 c = Vector2Add(a, Vector2Scale(ab, t));
            const float yMin = std::min(hb.origin.y, hb.end.y) - hb.radius, yMax = std::max(hb.origin.y, hb.end.y) + hb.radius;
            return Vector2Distance(c, p) <= hb.radius + hurt.radius &&
                   verticalOverlap(yMin, yMax, hurt.feet.y, hurt.feet.y + hurt.height);
        }
        case HitShape::Ring: {
            if (!verticalOverlap(hb.origin.y, hb.origin.y + hb.height, hurt.feet.y, hurt.feet.y + hurt.height)) return false;
            const float d = dist2D(hb.origin, hurt.feet);
            return d + hurt.radius >= hb.innerRadius && d - hurt.radius <= hb.radius;
        }
        case HitShape::Sector:
        default: {
            if (!verticalOverlap(hb.origin.y - 0.5f, hb.origin.y + hb.height, hurt.feet.y, hurt.feet.y + hurt.height))
                return false;
            const float d = dist2D(hb.origin, hurt.feet);
            if (d - hurt.radius > hb.radius || d + hurt.radius < hb.innerRadius) return false;
            if (hb.arcDeg >= 359.f || d < 1e-3f) return true;
            Vector2 f{hb.forward.x, hb.forward.z};
            const float fl = Vector2Length(f);
            if (fl < 1e-4f) return true;
            f = Vector2Scale(f, 1.f / fl);
            const Vector2 to{(hurt.feet.x - hb.origin.x) / d, (hurt.feet.z - hb.origin.z) / d};
            const float angle = std::acos(std::clamp(Vector2DotProduct(f, to), -1.f, 1.f));
            // Widen the arc by the angular size of the target so edge hits register.
            const float pad = std::atan2(hurt.radius, d);
            return angle <= hb.arcDeg * 0.5f * DEG2RAD + pad;
        }
    }
}

}  // namespace eng
