#pragma once
// In-run HUD: one panel per player in each corner (HP, special cooldown, relic icons),
// shared gold top-center, floor and room counter, revive prompts, floating damage numbers.
#include "engine/input/input_router.h"
#include "engine/render/renderer.h"
#include "engine/ui/ui.h"
#include "game/world/world.h"

namespace game {

struct HudInfo {
    int floor = 0;           // 0-based
    int room = 0, rooms = 0; // 1-based room counter
    const char* floorName = "";
    const char* roomType = "";
    float goldPulse = 0.f;
    const char* banner = nullptr;  // big centered message (ROOM CLEARED, ...)
    float bannerAlpha = 0.f;
};

void drawFloatingTexts(const eng::Ui& ui, const eng::Renderer& r, const World& w);
void drawHud(const eng::Ui& ui, const eng::Renderer& r, const World& w, const eng::InputRouter& input, const HudInfo& info);
void drawBossBar(const eng::Ui& ui, const char* name, float frac, float flash);

}  // namespace game
