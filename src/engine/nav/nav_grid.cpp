#include "engine/nav/nav_grid.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <raymath.h>

namespace eng {

void NavGrid::init(int width, int height, float cellSize, Vector3 origin) {
    w_ = std::max(width, 1);
    h_ = std::max(height, 1);
    cell_ = cellSize;
    origin_ = origin;
    blocked_.assign(static_cast<size_t>(w_ * h_), 0);
    g_.assign(blocked_.size(), 0.f);
    parent_.assign(blocked_.size(), -1);
    stamp_.assign(blocked_.size(), 0);
    search_ = 0;
}

void NavGrid::setBlocked(int x, int z, bool b) {
    if (x < 0 || z < 0 || x >= w_ || z >= h_) return;
    blocked_[static_cast<size_t>(idx(x, z))] = b ? 1 : 0;
}

void NavGrid::blockBox(Vector3 c, Vector3 he, float inflate) {
    const float x0 = c.x - he.x - inflate, x1 = c.x + he.x + inflate;
    const float z0 = c.z - he.z - inflate, z1 = c.z + he.z + inflate;
    for (int z = 0; z < h_; ++z)
        for (int x = 0; x < w_; ++x) {
            const Vector3 p = cellCenter(x, z);
            if (p.x >= x0 && p.x <= x1 && p.z >= z0 && p.z <= z1) setBlocked(x, z, true);
        }
}

bool NavGrid::blocked(int x, int z) const {
    if (x < 0 || z < 0 || x >= w_ || z >= h_) return true;
    return blocked_[static_cast<size_t>(idx(x, z))] != 0;
}

bool NavGrid::cellOf(Vector3 p, int& x, int& z) const {
    x = static_cast<int>(std::floor((p.x - origin_.x) / cell_));
    z = static_cast<int>(std::floor((p.z - origin_.z) / cell_));
    return x >= 0 && z >= 0 && x < w_ && z < h_;
}

bool NavGrid::walkable(Vector3 p) const {
    int x, z;
    return cellOf(p, x, z) && !blocked(x, z);
}

Vector3 NavGrid::cellCenter(int x, int z) const {
    return Vector3{origin_.x + (static_cast<float>(x) + 0.5f) * cell_, origin_.y,
                   origin_.z + (static_cast<float>(z) + 0.5f) * cell_};
}

bool NavGrid::lineClear(Vector3 a, Vector3 b) const {
    const float len = Vector2Distance(Vector2{a.x, a.z}, Vector2{b.x, b.z});
    const int steps = std::max(1, static_cast<int>(len / (cell_ * 0.35f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (!walkable(Vector3{a.x + (b.x - a.x) * t, 0.f, a.z + (b.z - a.z) * t})) return false;
    }
    return true;
}

Vector3 NavGrid::nearestWalkable(Vector3 p) const {
    int cx, cz;
    cellOf(p, cx, cz);
    cx = std::clamp(cx, 0, w_ - 1);
    cz = std::clamp(cz, 0, h_ - 1);
    if (!blocked(cx, cz)) return cellCenter(cx, cz);
    for (int r = 1; r < std::max(w_, h_); ++r)
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dz) != r) continue;
                if (!blocked(cx + dx, cz + dz)) return cellCenter(cx + dx, cz + dz);
            }
    return p;
}

bool NavGrid::findPath(Vector3 start, Vector3 goal, std::vector<Vector3>& out, int maxExpanded) const {
    out.clear();
    int sx, sz, gx, gz;
    if (!cellOf(start, sx, sz) || blocked(sx, sz)) {
        const Vector3 s = nearestWalkable(start);
        cellOf(s, sx, sz);
    }
    Vector3 goalPos = goal;
    if (!cellOf(goal, gx, gz) || blocked(gx, gz)) {
        goalPos = nearestWalkable(goal);
        cellOf(goalPos, gx, gz);
    }
    if (sx == gx && sz == gz) {
        out.push_back(goalPos);
        return true;
    }
    if (lineClear(start, goalPos)) {
        out.push_back(goalPos);
        return true;
    }
    ++search_;
    if (search_ == 0) {
        std::fill(stamp_.begin(), stamp_.end(), 0u);
        search_ = 1;
    }
    struct Node {
        float f;
        int i;
        bool operator>(const Node& o) const { return f > o.f; }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    auto h = [&](int x, int z) {
        const float dx = static_cast<float>(std::abs(x - gx)), dz = static_cast<float>(std::abs(z - gz));
        return (dx + dz) + (1.41421356f - 2.f) * std::min(dx, dz);  // octile
    };
    const int si = idx(sx, sz), gi = idx(gx, gz);
    stamp_[static_cast<size_t>(si)] = search_;
    g_[static_cast<size_t>(si)] = 0.f;
    parent_[static_cast<size_t>(si)] = -1;
    open.push(Node{h(sx, sz), si});
    int expanded = 0;
    bool found = false;
    static const int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int kDz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty() && expanded < maxExpanded) {
        const Node n = open.top();
        open.pop();
        if (n.i == gi) {
            found = true;
            break;
        }
        ++expanded;
        const int x = n.i % w_, z = n.i / w_;
        const float gCur = g_[static_cast<size_t>(n.i)];
        if (n.f - h(x, z) > gCur + 1e-3f) continue;  // stale entry
        for (int k = 0; k < 8; ++k) {
            const int nx = x + kDx[k], nz = z + kDz[k];
            if (blocked(nx, nz)) continue;
            if (k >= 4 && (blocked(x + kDx[k], z) || blocked(x, z + kDz[k]))) continue;  // no corner cutting
            const int ni = idx(nx, nz);
            const float ng = gCur + (k >= 4 ? 1.41421356f : 1.f);
            if (stamp_[static_cast<size_t>(ni)] == search_ && ng >= g_[static_cast<size_t>(ni)]) continue;
            stamp_[static_cast<size_t>(ni)] = search_;
            g_[static_cast<size_t>(ni)] = ng;
            parent_[static_cast<size_t>(ni)] = n.i;
            open.push(Node{ng + h(nx, nz), ni});
        }
    }
    if (!found) return false;
    std::vector<Vector3> cells;
    for (int i = gi; i != -1 && i != si; i = parent_[static_cast<size_t>(i)]) cells.push_back(cellCenter(i % w_, i / w_));
    std::reverse(cells.begin(), cells.end());
    if (!cells.empty()) cells.back() = goalPos;
    // String-pull: skip waypoints that are directly visible.
    Vector3 from = start;
    size_t i = 0;
    while (i < cells.size()) {
        size_t far = i;
        for (size_t j = cells.size(); j-- > i + 1;)
            if (lineClear(from, cells[j])) {
                far = j;
                break;
            }
        out.push_back(cells[far]);
        from = cells[far];
        i = far + 1;
    }
    return true;
}

Vector3 separation(Vector3 self, const std::vector<Vector3>& neighbors, float radius) {
    Vector3 push{0, 0, 0};
    for (const Vector3& n : neighbors) {
        const float dx = self.x - n.x, dz = self.z - n.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d < 1e-4f || d >= radius) continue;
        const float w = (radius - d) / radius;
        push.x += dx / d * w;
        push.z += dz / d * w;
    }
    return push;
}

}  // namespace eng
