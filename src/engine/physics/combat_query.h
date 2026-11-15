#pragma once
// Hitbox vs hurtbox overlap tests (run every fixed step) and per-attack hit lists so one
// swing hits each target at most once.
#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace eng {

// Hurtbox: vertical cylinder (character capsule approximation).
struct Hurtbox {
    Vector3 feet{};
    float radius = 0.5f;
    float height = 2.f;
};

enum class HitShape : uint8_t { Sector, Sphere, Ring, Capsule };

struct Hitbox {
    HitShape shape = HitShape::Sector;
    Vector3 origin{};       // sector/ring center (feet height), sphere center, capsule start
    Vector3 end{};          // capsule end
    Vector3 forward{0, 0, 1};
    float radius = 1.f;     // sector reach, sphere/capsule radius, ring outer radius
    float innerRadius = 0;  // ring inner radius / sector minimum distance
    float arcDeg = 90.f;    // sector full angle
    float height = 2.5f;    // vertical extent for sector/ring
};

bool overlaps(const Hitbox& hb, const Hurtbox& hurt);

// Remembers which targets an attack already hit.
class HitList {
public:
    void clear() { ids_.clear(); }
    bool contains(uint32_t id) const { return std::find(ids_.begin(), ids_.end(), id) != ids_.end(); }
    // Returns true if the id was newly added.
    bool add(uint32_t id) {
        if (contains(id)) return false;
        ids_.push_back(id);
        return true;
    }
    size_t size() const { return ids_.size(); }

private:
    std::vector<uint32_t> ids_;
};

}  // namespace eng
