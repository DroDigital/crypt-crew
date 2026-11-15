#pragma once
// PCG32: small, fast, seedable. Same sequence on every platform.
#include <cstdint>

namespace eng {

class Rng {
public:
    explicit Rng(uint64_t seed = 0x853c49e6748fea9bULL) { reseed(seed); }
    void reseed(uint64_t seed) {
        state_ = 0;
        next();
        state_ += seed;
        next();
    }
    uint32_t next() {
        uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }
    // [0, n)
    int range(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n)); }
    // [lo, hi] inclusive
    int rangeInclusive(int lo, int hi) { return lo + range(hi - lo + 1); }
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    bool chance(float p) { return uniform() < p; }

private:
    uint64_t state_ = 0;
};

}  // namespace eng
