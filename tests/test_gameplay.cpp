#include <doctest/doctest.h>

#include "engine/input/input_router.h"
#include "engine/nav/nav_grid.h"
#include "engine/physics/combat_query.h"
#include "game/core/attack_tokens.h"
#include "game/core/game_state.h"

TEST_CASE("revive cost doubles with each revive on the same floor and resets per floor") {
    game::GameState s;
    s.startRun(1);
    CHECK(s.reviveCost(25, 2) == 25);
    s.recordRevive();
    CHECK(s.reviveCost(25, 2) == 50);
    s.recordRevive();
    CHECK(s.reviveCost(25, 2) == 100);
    s.startFloor(1);
    CHECK(s.reviveCost(25, 2) == 25);
    CHECK(s.revivesUsed == 2);
}

TEST_CASE("not enough gold means no revive") {
    game::GameState s;
    s.startRun(1);
    s.addGold(30);
    CHECK(s.canAfford(s.reviveCost(25, 2)));
    CHECK(s.spend(25));
    s.recordRevive();
    CHECK_FALSE(s.canAfford(s.reviveCost(25, 2)));
    CHECK_FALSE(s.spend(50));
    CHECK(s.purse == 5);
    CHECK(s.goldEarned == 30);
}

TEST_CASE("attack tokens cap attackers per player") {
    game::AttackTokens t(3);
    CHECK(t.acquire(1, 10));
    CHECK(t.acquire(1, 11));
    CHECK(t.acquire(1, 12));
    CHECK_FALSE(t.acquire(1, 13));
    CHECK(t.acquire(1, 12));  // re-acquire is fine
    CHECK(t.count(1) == 3);
    CHECK(t.acquire(2, 13));  // other player
    t.release(1, 11);
    CHECK(t.acquire(1, 13));
    t.releaseAll(13);
    CHECK(t.count(1) == 2);
    CHECK(t.count(2) == 0);
    CHECK(t.holds(1, 10));
    CHECK_FALSE(t.holds(1, 13));
}

namespace {
eng::RawDeviceState pad(bool connected, uint32_t down = 0) {
    eng::RawDeviceState d;
    d.connected = connected;
    d.down = down;
    return d;
}
}  // namespace

TEST_CASE("InputRouter join and leave") {
    eng::InputRouter r;
    eng::RawDeviceState dev[eng::kMaxDevices];
    dev[0] = pad(true);
    dev[0].family = eng::PadFamily::Keyboard;
    for (int i = 1; i < eng::kMaxDevices; ++i) dev[i] = pad(i <= 2);
    r.update(dev);
    // Pad 1 presses A -> join edge.
    dev[1].down = eng::kBtnJoin | eng::kBtnConfirm;
    r.update(dev);
    CHECK(r.anyDevicePressed(eng::kBtnJoin) == 1);
    CHECK(r.join(1) == 0);
    CHECK(r.join(1) == -1);  // same device can't join twice
    CHECK(r.join(0) == 1);   // keyboard
    CHECK(r.activeSlots() == 2);
    CHECK(r.slotOfDevice(0) == 1);
    // The join press does not leak into gameplay.
    r.update(dev);
    CHECK((r.consumePressed(0) & eng::kBtnConfirm) == 0);
    // Leave frees the slot for the next joiner.
    r.leave(0);
    CHECK(r.activeSlots() == 1);
    CHECK(r.join(2) == 0);
    CHECK(r.join(3) == 2);
    CHECK(r.join(4) == 3);
    CHECK(r.join(1) == -1);  // full
}

TEST_CASE("InputRouter latches presses until consumed and flags disconnects") {
    eng::InputRouter r;
    eng::RawDeviceState dev[eng::kMaxDevices];
    for (int i = 0; i < eng::kMaxDevices; ++i) dev[i] = pad(true);
    r.update(dev);
    const int s = r.join(2);
    dev[2].down = eng::kBtnAttack;
    r.update(dev);
    dev[2].down = 0;
    r.update(dev);  // released before the sim stepped (e.g. hitstop frame)
    CHECK((r.consumePressed(s) & eng::kBtnAttack) != 0);
    CHECK(r.consumePressed(s) == 0);
    dev[2].connected = false;
    r.update(dev);
    CHECK(r.slotDisconnected(s));
    CHECK(r.firstDisconnectedSlot() == s);
    dev[2].connected = true;
    r.update(dev);
    CHECK_FALSE(r.slotDisconnected(s));
}

TEST_CASE("hit shapes and per-attack hit lists") {
    eng::Hitbox sector;
    sector.shape = eng::HitShape::Sector;
    sector.origin = {0, 0, 0};
    sector.forward = {0, 0, 1};
    sector.radius = 2.f;
    sector.arcDeg = 90.f;
    CHECK(eng::overlaps(sector, eng::Hurtbox{{0, 0, 1.5f}, 0.5f, 2.f}));
    CHECK_FALSE(eng::overlaps(sector, eng::Hurtbox{{0, 0, -1.5f}, 0.5f, 2.f}));  // behind
    CHECK_FALSE(eng::overlaps(sector, eng::Hurtbox{{0, 0, 3.f}, 0.4f, 2.f}));    // out of reach
    eng::Hitbox ring;
    ring.shape = eng::HitShape::Ring;
    ring.radius = 5.f;
    ring.innerRadius = 4.f;
    ring.height = 1.f;
    CHECK(eng::overlaps(ring, eng::Hurtbox{{4.5f, 0, 0}, 0.4f, 2.f}));
    CHECK_FALSE(eng::overlaps(ring, eng::Hurtbox{{1.f, 0, 0}, 0.4f, 2.f}));  // inside the ring: safe
    eng::HitList hl;
    CHECK(hl.add(7));
    CHECK_FALSE(hl.add(7));
    CHECK(hl.size() == 1);
}

TEST_CASE("A* finds a path around a wall") {
    eng::NavGrid g;
    g.init(10, 10, 1.f, {0, 0, 0});
    for (int z = 0; z < 8; ++z) g.setBlocked(5, z, true);
    std::vector<Vector3> path;
    REQUIRE(g.findPath({1.5f, 0, 1.5f}, {8.5f, 0, 1.5f}, path));
    REQUIRE_FALSE(path.empty());
    for (const auto& p : path) CHECK(g.walkable(p));
    CHECK(path.back().x == doctest::Approx(8.5f));
    for (int z = 0; z < 10; ++z) g.setBlocked(5, z, true);
    CHECK_FALSE(g.findPath({1.5f, 0, 1.5f}, {8.5f, 0, 1.5f}, path));
}

TEST_CASE("InputRouter can join into a preferred free slot (bots hold the others)") {
    eng::InputRouter r;
    eng::RawDeviceState dev[eng::kMaxDevices];
    for (int i = 0; i < eng::kMaxDevices; ++i) dev[i] = pad(true);
    r.update(dev);
    CHECK(r.join(1, 2) == 2);
    CHECK(r.join(2, 2) == -1);  // taken
    CHECK(r.join(2, 0) == 0);
    CHECK(r.slotDevice(2) == 1);
}
