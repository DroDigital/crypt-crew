#pragma once
// Limits how many enemies may attack the same player at once.
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace game {

class AttackTokens {
public:
    explicit AttackTokens(int maxPerTarget = 3) : max_(maxPerTarget) {}
    void setMax(int m) { max_ = m; }
    // True if `holder` now holds (or already held) a token on `target`.
    bool acquire(uint32_t target, uint32_t holder) {
        auto& v = held_[target];
        if (std::find(v.begin(), v.end(), holder) != v.end()) return true;
        if (static_cast<int>(v.size()) >= max_) return false;
        v.push_back(holder);
        return true;
    }
    void release(uint32_t target, uint32_t holder) {
        auto it = held_.find(target);
        if (it == held_.end()) return;
        auto& v = it->second;
        v.erase(std::remove(v.begin(), v.end(), holder), v.end());
    }
    void releaseAll(uint32_t holder) {
        for (auto& [t, v] : held_) v.erase(std::remove(v.begin(), v.end(), holder), v.end());
    }
    int count(uint32_t target) const {
        auto it = held_.find(target);
        return it == held_.end() ? 0 : static_cast<int>(it->second.size());
    }
    bool holds(uint32_t target, uint32_t holder) const {
        auto it = held_.find(target);
        return it != held_.end() && std::find(it->second.begin(), it->second.end(), holder) != it->second.end();
    }
    void clear() { held_.clear(); }

private:
    int max_;
    std::map<uint32_t, std::vector<uint32_t>> held_;
};

}  // namespace game
