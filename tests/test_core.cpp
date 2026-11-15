#include <doctest/doctest.h>

#include "engine/core/fixed_clock.h"
#include "engine/core/rng.h"
#include "game/data/game_data.h"

TEST_CASE("fixed clock runs 60 steps per second and freezes during hitstop") {
    eng::FixedClock c(60.0);
    int steps = 0;
    for (int i = 0; i < 60; ++i) steps += c.advance(1.0 / 60.0);
    CHECK(steps == 60);
    c.hitstop(0.1);
    int frozen = 0;
    for (int i = 0; i < 6; ++i) frozen += c.advance(1.0 / 60.0);
    CHECK(frozen == 0);
    int after = 0;
    for (int i = 0; i < 60; ++i) after += c.advance(1.0 / 60.0);
    CHECK(after >= 59);
}

TEST_CASE("slow motion scales simulation time, not frames") {
    eng::FixedClock c(60.0);
    c.setTimeScale(0.5);
    int steps = 0;
    for (int i = 0; i < 60; ++i) steps += c.advance(1.0 / 60.0);
    CHECK(steps == doctest::Approx(30).epsilon(0.05));
}

TEST_CASE("rng is deterministic per seed") {
    eng::Rng a(42), b(42), c(43);
    bool differs = false;
    for (int i = 0; i < 100; ++i) {
        const uint32_t x = a.next();
        CHECK(x == b.next());
        if (x != c.next()) differs = true;
    }
    CHECK(differs);
    eng::Rng r(7);
    for (int i = 0; i < 1000; ++i) {
        const int v = r.rangeInclusive(3, 5);
        CHECK(v >= 3);
        CHECK(v <= 5);
    }
}

TEST_CASE("enemy scaling by player count") {
    game::ScalingDef s;
    s.enemyHpMult = {1.0f, 1.5f, 1.9f, 2.3f};
    s.spawnBonusPerPlayer = 0.3f;
    CHECK(s.hpMult(1) == doctest::Approx(1.0f));
    CHECK(s.hpMult(2) == doctest::Approx(1.5f));
    CHECK(s.hpMult(3) == doctest::Approx(1.9f));
    CHECK(s.hpMult(4) == doctest::Approx(2.3f));
    CHECK(s.hpMult(9) == doctest::Approx(2.3f));
    CHECK(s.spawnCount(10, 1) == 10);
    CHECK(s.spawnCount(10, 2) == 13);
    CHECK(s.spawnCount(10, 4) == 19);
    CHECK(s.spawnCount(4, 3) == 6);
}
