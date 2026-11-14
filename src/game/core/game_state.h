#pragma once
// Run-wide state (service created in main): seed, floor, shared purse, relics, revives, stats.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct PlayerRecord {
    bool active = false;
    std::string classId;
    int device = -1;
    std::vector<std::string> relics;
    // Run summary.
    int kills = 0;
    float damageDealt = 0.f;
    int downs = 0;
    int revivesGiven = 0;
    bool died = false;
    std::string killedBy;
};

class GameState {
public:
    void startRun(uint64_t seed);
    void startFloor(int floor);

    // Revive cost: base, doubling (multiplier) with each revive on the same floor.
    int reviveCost(int base, int multiplier) const;
    bool canAfford(int cost) const { return purse >= cost; }
    bool spend(int cost);
    void addGold(int amount);
    void recordRevive() {
        ++revivesThisFloor;
        ++revivesUsed;
    }
    int activePlayers() const;

    uint64_t seed = 0;
    int floor = 0;          // 0-based
    int roomIndex = 0;
    int purse = 0;
    int goldEarned = 0;
    int revivesThisFloor = 0;
    int revivesUsed = 0;
    int roomsCleared = 0;
    double runTime = 0.0;
    bool victory = false;
    bool statueUsed = false;
    std::array<PlayerRecord, 4> players{};
    std::vector<std::string> unlockedRelics;
};

}  // namespace game
