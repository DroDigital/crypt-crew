#include "game/rooms/floor_generator.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>

#include "engine/core/rng.h"

namespace game {
namespace {

const int kDx[4] = {0, 1, 0, -1};  // N E S W (north = -z = up on screen)
const int kDz[4] = {-1, 0, 1, 0};

std::vector<std::string> templatesOf(const GameData& d, RoomType t) {
    std::vector<std::string> out;
    for (const auto& [id, r] : d.rooms)
        if (r.type == t) out.push_back(id);
    return out;
}

std::string pick(eng::Rng& rng, std::vector<std::string>& pool, std::vector<std::string>& used) {
    std::vector<std::string> fresh;
    for (const auto& id : pool)
        if (std::find(used.begin(), used.end(), id) == used.end()) fresh.push_back(id);
    const auto& from = fresh.empty() ? pool : fresh;
    if (from.empty()) return {};
    const std::string id = from[static_cast<size_t>(rng.range(static_cast<int>(from.size())))];
    used.push_back(id);
    return id;
}

}  // namespace

FloorPlan generateFloor(const GameData& data, int floor, uint64_t seed) {
    eng::Rng rng(seed * 0x9E3779B97F4A7C15ULL + static_cast<uint64_t>(floor) * 7919ULL + 1ULL);
    const FloorTheme& theme = data.floors[static_cast<size_t>(std::clamp(floor, 0, static_cast<int>(data.floors.size()) - 1))];
    const bool last = floor >= static_cast<int>(data.floors.size()) - 1;
    const int total = rng.rangeInclusive(std::max(theme.rooms[0], 6), std::min(std::max(theme.rooms[1], 6), 8));
    const int mainLen = total - 1;  // one room goes to the side branch

    for (int attempt = 0; attempt < 64; ++attempt) {
        FloorPlan plan;
        std::set<std::pair<int, int>> taken;
        int x = 0, z = 0;
        taken.insert({x, z});
        std::vector<std::pair<int, int>> cells{{x, z}};
        std::vector<int> dirs;
        bool ok = true;
        for (int i = 1; i < mainLen && ok; ++i) {
            // Mostly north, sometimes east/west, never south (keeps the chain readable).
            std::vector<int> options;
            for (int d : {0, 0, 1, 3})
                if (!taken.count({x + kDx[d], z + kDz[d]})) options.push_back(d);
            if (options.empty()) {
                ok = false;
                break;
            }
            const int d = options[static_cast<size_t>(rng.range(static_cast<int>(options.size())))];
            x += kDx[d];
            z += kDz[d];
            taken.insert({x, z});
            cells.push_back({x, z});
            dirs.push_back(d);
        }
        if (!ok) continue;
        // Branch: a free neighbor of a middle room (not the first or last).
        std::vector<std::pair<int, int>> branchCands;  // (room index, side)
        for (int i = 1; i < mainLen - 1; ++i)
            for (int d = 0; d < 4; ++d)
                if (!taken.count({cells[static_cast<size_t>(i)].first + kDx[d], cells[static_cast<size_t>(i)].second + kDz[d]}))
                    branchCands.push_back({i, d});
        if (branchCands.empty()) continue;
        const auto [bi, bd] = branchCands[static_cast<size_t>(rng.range(static_cast<int>(branchCands.size())))];

        // Room types along the chain.
        std::vector<std::string> used;
        auto combat = templatesOf(data, RoomType::Combat);
        auto trap = templatesOf(data, RoomType::Trap);
        auto treasure = templatesOf(data, RoomType::Treasure);
        auto shrine = templatesOf(data, RoomType::Shrine);
        auto boss = templatesOf(data, RoomType::Boss);
        const int trapIndex = mainLen >= 5 ? rng.rangeInclusive(2, mainLen - 3) : -1;
        for (int i = 0; i < mainLen; ++i) {
            RoomNode n;
            n.gx = cells[static_cast<size_t>(i)].first;
            n.gz = cells[static_cast<size_t>(i)].second;
            if (i == mainLen - 1) n.type = last ? RoomType::Boss : RoomType::Shrine;
            else if (i == trapIndex && !trap.empty()) n.type = RoomType::Trap;
            else n.type = RoomType::Combat;
            auto& pool = n.type == RoomType::Boss ? boss : n.type == RoomType::Shrine ? shrine
                         : n.type == RoomType::Trap ? trap : combat;
            n.templateId = pick(rng, pool, used);
            if (n.templateId.empty()) {
                ok = false;
                break;
            }
            plan.rooms.push_back(n);
        }
        if (!ok) continue;
        for (int i = 0; i + 1 < mainLen; ++i) {
            const int d = dirs[static_cast<size_t>(i)];
            plan.rooms[static_cast<size_t>(i)].neighbor[static_cast<size_t>(d)] = i + 1;
            plan.rooms[static_cast<size_t>(i + 1)].neighbor[static_cast<size_t>((d + 2) % 4)] = i;
        }
        RoomNode b;
        b.type = RoomType::Treasure;
        b.branch = true;
        b.gx = cells[static_cast<size_t>(bi)].first + kDx[bd];
        b.gz = cells[static_cast<size_t>(bi)].second + kDz[bd];
        b.templateId = pick(rng, treasure, used);
        if (b.templateId.empty()) continue;
        b.neighbor[static_cast<size_t>((bd + 2) % 4)] = bi;
        plan.rooms[static_cast<size_t>(bi)].neighbor[static_cast<size_t>(bd)] = static_cast<int>(plan.rooms.size());
        plan.rooms.push_back(b);
        plan.start = 0;
        plan.exit = mainLen - 1;
        // Sockets: every template must have a door on each side we connect.
        bool sockets = true;
        for (const auto& r : plan.rooms)
            for (int s = 0; s < 4; ++s)
                if (r.neighbor[static_cast<size_t>(s)] >= 0 && !data.room(r.templateId).hasDoor(static_cast<Side>(s))) sockets = false;
        if (!sockets) continue;
        return plan;
    }
    // Fallback (should not happen with the shipped templates): a straight line north.
    FloorPlan plan;
    for (int i = 0; i < 6; ++i) {
        RoomNode n;
        n.gz = -i;
        n.type = i == 5 ? (last ? RoomType::Boss : RoomType::Shrine) : RoomType::Combat;
        n.templateId = templatesOf(data, n.type).empty() ? data.rooms.begin()->first : templatesOf(data, n.type).front();
        if (i > 0) n.neighbor[kSouth] = i - 1;
        if (i < 5) n.neighbor[kNorth] = i + 1;
        plan.rooms.push_back(n);
    }
    plan.exit = 5;
    return plan;
}

std::string validatePlan(const FloorPlan& plan, int minRooms, int maxRooms) {
    const int n = static_cast<int>(plan.rooms.size());
    if (n < minRooms || n > maxRooms) return "room count " + std::to_string(n);
    int branches = 0;
    std::set<std::pair<int, int>> cells;
    for (int i = 0; i < n; ++i) {
        const RoomNode& r = plan.rooms[static_cast<size_t>(i)];
        branches += r.branch ? 1 : 0;
        if (!cells.insert({r.gx, r.gz}).second) return "overlapping rooms";
        for (int s = 0; s < 4; ++s) {
            const int nb = r.neighbor[static_cast<size_t>(s)];
            if (nb < 0) continue;
            if (nb >= n) return "bad neighbor index";
            const RoomNode& o = plan.rooms[static_cast<size_t>(nb)];
            if (o.neighbor[static_cast<size_t>((s + 2) % 4)] != i) return "asymmetric door";
            if (o.gx != r.gx + kDx[s] || o.gz != r.gz + kDz[s]) return "neighbor not adjacent on the grid";
        }
    }
    if (branches != 1) return "expected exactly one branch, got " + std::to_string(branches);
    if (plan.rooms[static_cast<size_t>(plan.start)].branch) return "start is a branch";
    // Connected (BFS from start).
    std::vector<bool> seen(static_cast<size_t>(n), false);
    std::queue<int> q;
    q.push(plan.start);
    seen[static_cast<size_t>(plan.start)] = true;
    int count = 0;
    while (!q.empty()) {
        const int c = q.front();
        q.pop();
        ++count;
        for (int nb : plan.rooms[static_cast<size_t>(c)].neighbor)
            if (nb >= 0 && !seen[static_cast<size_t>(nb)]) {
                seen[static_cast<size_t>(nb)] = true;
                q.push(nb);
            }
    }
    if (count != n) return "not connected";
    // The branch is a dead end hanging off the main chain.
    for (const auto& r : plan.rooms)
        if (r.branch) {
            int links = 0;
            for (int nb : r.neighbor) links += nb >= 0 ? 1 : 0;
            if (links != 1) return "branch must have exactly one door";
        }
    return {};
}

}  // namespace game
