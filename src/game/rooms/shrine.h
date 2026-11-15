#pragma once
// Shrine room: a statue that fully heals the team once per floor and a merchant who sells
// heals and relics for the shared purse (data/shrine.json). Each player shops on their own panel.
#include <array>
#include <string>
#include <vector>

#include "engine/input/input_router.h"
#include "engine/ui/ui.h"
#include "game/world/world.h"

namespace game {

class Shrine {
public:
    // Sets up statue/merchant from the room's markers. Call on entering a shrine room.
    void enter(World& w, const RoomTemplate& tpl);
    void leave() { active_ = false; }
    bool active() const { return active_; }
    bool shopping(int slot) const { return active_ && shops_[static_cast<size_t>(slot & 3)].open; }

    void fixedUpdate(World& w, const std::array<PlayerCommand, 4>& cmds, float dt);
    void frameUpdate(World& w, eng::InputRouter& in);
    void render(eng::Renderer& r, World& w, float alpha);
    void drawUi(const eng::Ui& ui, const eng::Renderer& r, const World& w, const eng::InputRouter& in) const;
    Vector3 merchantPos() const { return merchantPos_; }

private:
    struct Shop {
        bool open = false;
        bool justOpened = false;  // the opening press must not also close it
        int focus = 0;
    };
    struct Item {
        bool heal = false;
        std::string relic;
        int price = 0;
        bool sold = false;
    };
    void buy(World& w, Player& p, Item& item);
    bool active_ = false;
    eng::json cfg_;
    Vector3 statuePos_{}, merchantPos_{};
    bool hasStatue_ = false, hasMerchant_ = false;
    CharacterVisual merchant_;
    std::vector<Item> stock_;
    std::array<Shop, 4> shops_{};
    float statueGlow_ = 0.f;
};

}  // namespace game
