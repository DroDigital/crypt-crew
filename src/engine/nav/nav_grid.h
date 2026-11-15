#pragma once
// A* on a room's walkable grid (built from the same tile data as meshes and colliders).
#include <raylib.h>

#include <cstdint>
#include <vector>

namespace eng {

class NavGrid {
public:
    void init(int width, int height, float cellSize, Vector3 origin);
    void setBlocked(int x, int z, bool blocked);
    void blockBox(Vector3 center, Vector3 halfExtents, float inflate);  // axis-aligned in world space
    bool blocked(int x, int z) const;
    bool walkable(Vector3 p) const;
    bool cellOf(Vector3 p, int& x, int& z) const;
    Vector3 cellCenter(int x, int z) const;
    // Straight walkable line between two points (grid DDA)?
    bool lineClear(Vector3 a, Vector3 b) const;
    // Path from start to goal; waypoints exclude the start, end at goal (or nearest walkable cell).
    // Returns false if unreachable. maxExpanded bounds the search cost.
    bool findPath(Vector3 start, Vector3 goal, std::vector<Vector3>& out, int maxExpanded = 4000) const;
    // Nearest walkable cell center to p (spiral search).
    Vector3 nearestWalkable(Vector3 p) const;
    int width() const { return w_; }
    int height() const { return h_; }
    float cellSize() const { return cell_; }

private:
    int idx(int x, int z) const { return z * w_ + x; }
    int w_ = 0, h_ = 0;
    float cell_ = 1.f;
    Vector3 origin_{};
    std::vector<uint8_t> blocked_;
    mutable std::vector<float> g_;
    mutable std::vector<int> parent_;
    mutable std::vector<uint32_t> stamp_;
    mutable uint32_t search_ = 0;
};

// Separation steering: pushes away from nearby agents so swarms don't stack.
Vector3 separation(Vector3 self, const std::vector<Vector3>& neighbors, float radius);

}  // namespace eng
