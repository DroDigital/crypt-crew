#include <doctest/doctest.h>

#include <set>

#include "game/data/game_data.h"
#include "game/data/relic_defs.h"
#include "game/rooms/floor_generator.h"

namespace {
const game::GameData& data() {
    static game::GameData d = [] {
        game::GameData g;
        g.loadAll();  // the real data/ folder (found by walking up from the test binary)
        return g;
    }();
    return d;
}
}  // namespace

TEST_CASE("data: 12 room templates of the right types, 15 relics") {
    const auto& d = data();
    std::map<game::RoomType, int> counts;
    for (const auto& [id, r] : d.rooms) {
        ++counts[r.type];
        for (int s = 0; s < 4; ++s) CHECK_MESSAGE(r.hasDoor(static_cast<game::Side>(s)), id << " needs all four sockets");
    }
    CHECK(d.rooms.size() == 12);
    CHECK(counts[game::RoomType::Combat] == 7);
    CHECK(counts[game::RoomType::Treasure] == 2);
    CHECK(counts[game::RoomType::Trap] == 1);
    CHECK(counts[game::RoomType::Shrine] == 1);
    CHECK(counts[game::RoomType::Boss] == 1);
    CHECK(d.relics.size() == 15);
}

TEST_CASE("floor generator: seeded, connected, 6-8 rooms, one branch") {
    const auto& d = data();
    for (int floor = 0; floor < 3; ++floor) {
        for (uint64_t seed = 1; seed <= 200; ++seed) {
            const game::FloorPlan plan = game::generateFloor(d, floor, seed);
            const std::string err = game::validatePlan(plan, 6, 8);
            REQUIRE_MESSAGE(err.empty(), "floor " << floor << " seed " << seed << ": " << err);
            const auto& exit = plan.rooms[static_cast<size_t>(plan.exit)];
            CHECK(exit.type == (floor == 2 ? game::RoomType::Boss : game::RoomType::Shrine));
            CHECK(plan.rooms[static_cast<size_t>(plan.start)].type == game::RoomType::Combat);
        }
    }
    // Same seed -> same floor; different seeds -> some variety.
    const auto a = game::generateFloor(d, 0, 42), b = game::generateFloor(d, 0, 42);
    REQUIRE(a.rooms.size() == b.rooms.size());
    for (size_t i = 0; i < a.rooms.size(); ++i) {
        CHECK(a.rooms[i].templateId == b.rooms[i].templateId);
        CHECK(a.rooms[i].gx == b.rooms[i].gx);
        CHECK(a.rooms[i].gz == b.rooms[i].gz);
    }
    std::set<std::string> layouts;
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        std::string key;
        for (const auto& r : game::generateFloor(d, 1, seed).rooms) key += r.templateId + std::to_string(r.gx) + "," + std::to_string(r.gz) + ";";
        layouts.insert(key);
    }
    CHECK(layouts.size() > 10);
}

TEST_CASE("relic stacking") {
    const auto& d = data();
    game::PlayerStats s = game::computeStats(d, {"swift_boots", "swift_boots", "heart_stone"});
    CHECK(s.moveMult == doctest::Approx(1.30f));
    CHECK(s.maxHpBonus == doctest::Approx(20.f));
    s = game::computeStats(d, {"hourglass", "hourglass"});
    CHECK(s.specialCdMult == doctest::Approx(0.6f));
    s = game::computeStats(d, {"hourglass", "hourglass", "hourglass", "hourglass", "hourglass"});
    CHECK(s.specialCdMult >= 0.2f);  // clamped
    s = game::computeStats(d, {"second_wind", "second_wind"});
    CHECK(s.extraDodges == 2);
    CHECK(game::hooksOf(d, {"ember_soles", "swift_boots", "powder_keg"}) == std::vector<std::string>{"fire_trail", "bomb_on_death"});
    // Offers respect class restrictions and non-stackable relics already owned.
    eng::Rng rng(5);
    for (int i = 0; i < 200; ++i) {
        const auto offer = game::rollRelicOffer(d, "knight", {"ember_soles"}, rng, 3);
        CHECK(offer.size() == 3);
        for (const auto& id : offer) {
            CHECK(id != "twin_staff");
            CHECK(id != "ember_soles");
        }
        CHECK(std::set<std::string>(offer.begin(), offer.end()).size() == 3);
    }
}
