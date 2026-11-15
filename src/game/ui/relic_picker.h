#pragma once
// After a chest opens, every player picks 1 of 3 relics on their own panel, at the same time.
#include <array>
#include <string>
#include <vector>

#include "engine/input/input_router.h"
#include "engine/ui/ui.h"
#include "game/world/world.h"

namespace game {

class RelicPicker {
public:
    // Opens a pick for every player that is not dead.
    void open(World& w);
    bool active() const;
    bool picking(int slot) const { return slots_[static_cast<size_t>(slot & 3)].open; }
    // Per-frame input (menus use frame edges). Bots pick after a short delay.
    void update(World& w, eng::InputRouter& in, float realDt);
    void draw(const eng::Ui& ui, const World& w, const eng::InputRouter& in) const;
    void close();

private:
    struct SlotPick {
        bool open = false;
        std::vector<std::string> options;
        int focus = 0;
        float timer = 0.f;
        float appear = 0.f;
    };
    void choose(World& w, Player& p, const std::string& relicId);
    std::array<SlotPick, 4> slots_{};
};

// Applies a newly owned relic to the player (stats, hooks, HP).
void grantRelic(World& w, Player& p, const std::string& relicId);

}  // namespace game
