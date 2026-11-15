#include "game/core/game_state.h"

#include <climits>

namespace game {

void GameState::startRun(uint64_t s) {
    const auto keepPlayers = players;
    const auto keepUnlocked = unlockedRelics;
    *this = GameState{};
    seed = s;
    unlockedRelics = keepUnlocked;
    for (size_t i = 0; i < players.size(); ++i) {
        players[i].active = keepPlayers[i].active;
        players[i].classId = keepPlayers[i].classId;
        players[i].device = keepPlayers[i].device;
    }
}

void GameState::startFloor(int f) {
    floor = f;
    roomIndex = 0;
    revivesThisFloor = 0;
    statueUsed = false;
}

int GameState::reviveCost(int base, int multiplier) const {
    long long cost = base;
    for (int i = 0; i < revivesThisFloor; ++i) {
        cost *= multiplier;
        if (cost > INT_MAX / 4) return INT_MAX / 4;
    }
    return static_cast<int>(cost);
}

bool GameState::spend(int cost) {
    if (purse < cost) return false;
    purse -= cost;
    return true;
}

void GameState::addGold(int amount) {
    purse += amount;
    goldEarned += amount;
}

int GameState::activePlayers() const {
    int n = 0;
    for (const auto& p : players) n += p.active ? 1 : 0;
    return n;
}

}  // namespace game
