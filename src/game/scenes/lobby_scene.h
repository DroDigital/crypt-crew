#pragma once
// Lobby: press Start/A (or Enter) on any device to join, pick a class, ready up. Back leaves.
// One or more ready players start the run. F2 adds an AI teammate (debug).
#include <array>

#include "game/core/app.h"
#include "game/visual/character_visual.h"

namespace game {

class LobbyScene : public Scene {
public:
    explicit LobbyScene(App& app, int preJoinedBots = 0);
    ~LobbyScene() override;
    void fixedUpdate(float dt) override;
    void frameUpdate(float realDt) override;
    void render(float alpha) override;
    const char* name() const override { return "lobby"; }

private:
    struct SlotUi {
        bool joined = false;
        bool bot = false;
        bool ready = false;
        int classIdx = 0;
        CharacterVisual vis;
        RenderTexture2D rt{};
        float spin = 0.f;
        float pop = 0.f;
    };
    void setClass(int slot, int idx);
    int firstFreeClass() const;
    void addBot();
    void startRun();
    App& app_;
    std::array<SlotUi, 4> slots_;
    float countdown_ = -1.f;
    float time_ = 0.f;
};

}  // namespace game
