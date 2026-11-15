#include "game/scenes/summary_scene.h"

#include <cmath>
#include <cstdio>

#include "game/data/relic_defs.h"
#include "game/scenes/lobby_scene.h"
#include "game/ui/glyphs.h"

namespace game {

SummaryScene::SummaryScene(App& app) : app_(app) {
    app_.audio().stopMusic(1.f);
    app_.audio().play(app_.state().victory ? "relic" : "downed");
}

void SummaryScene::frameUpdate(float) {
    if (time_ < 1.f) return;  // don't skip it by accident
    eng::InputRouter& in = app_.input();
    if (in.anyDevicePressed(eng::kBtnConfirm | eng::kBtnPause) >= 0) {
        app_.audio().play("ui_confirm");
        app_.switchTo(std::make_unique<LobbyScene>(app_));
    }
}

void SummaryScene::render(float) {
    const eng::Ui& ui = app_.ui();
    const GameState& st = app_.state();
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           st.victory ? Color{40, 32, 18, 255} : Color{30, 10, 14, 255}, Color{6, 5, 10, 255});
    const float a = std::min(1.f, time_ * 1.5f);
    ui.text(st.victory ? "VICTORY" : "THE CREW HAS FALLEN", Vector2{ui.width() * 0.5f, 60}, 92,
            st.victory ? Color{255, 220, 120, static_cast<unsigned char>(255 * a)} : Color{230, 90, 80, static_cast<unsigned char>(255 * a)},
            eng::Align::Center);
    if (st.victory)
        ui.text("The Bone King is dust. The crypt is quiet again.", Vector2{ui.width() * 0.5f, 170}, 28, Color{220, 210, 190, 255},
                eng::Align::Center);
    char buf[128];
    const int mins = static_cast<int>(st.runTime) / 60, secs = static_cast<int>(st.runTime) % 60;
    std::snprintf(buf, sizeof buf, "Time %d:%02d     Floor reached %d     Rooms cleared %d     Gold earned %d     Revives used %d",
                  mins, secs, st.floor + 1, st.roomsCleared, st.goldEarned, st.revivesUsed);
    ui.text(buf, Vector2{ui.width() * 0.5f, 220}, 28, Color{230, 230, 240, 255}, eng::Align::Center);

    // One card per player.
    int n = 0;
    for (const auto& p : st.players) n += p.active ? 1 : 0;
    const float cw = 400, ch = 470, gap = 28;
    float x = (ui.width() - (cw * static_cast<float>(n) + gap * static_cast<float>(n - 1))) * 0.5f;
    for (int i = 0; i < 4; ++i) {
        const PlayerRecord& p = st.players[static_cast<size_t>(i)];
        if (!p.active) continue;
        const Rectangle r{x, 290, cw, ch};
        x += cw + gap;
        ui.panel(r, slotColor(i));
        const ClassDef* cls = app_.data().classes.count(p.classId) ? &app_.data().cls(p.classId) : nullptr;
        std::snprintf(buf, sizeof buf, "P%d  %s", i + 1, cls ? cls->name.c_str() : "?");
        ui.text(buf, Vector2{r.x + 20, r.y + 16}, 34, slotColor(i));
        float y = r.y + 72;
        auto line = [&](const char* label, const std::string& value) {
            ui.text(label, Vector2{r.x + 24, y}, 24, Color{180, 180, 195, 255});
            ui.text(value.c_str(), Vector2{r.x + cw - 24, y}, 24, RAYWHITE, eng::Align::Right);
            y += 36;
        };
        line("Kills", std::to_string(p.kills));
        line("Damage dealt", std::to_string(static_cast<int>(p.damageDealt)));
        line("Times downed", std::to_string(p.downs));
        line("Revives given", std::to_string(p.revivesGiven));
        y += 8;
        const std::string fate = p.died ? "Fell to " + (p.killedBy.empty() ? std::string("the crypt") : p.killedBy)
                                        : (st.victory ? std::string("Survived") : std::string("Last stand"));
        ui.text(fate.c_str(), Vector2{r.x + cw * 0.5f, y}, 22, p.died ? Color{240, 110, 100, 255} : Color{140, 240, 150, 255},
                eng::Align::Center);
        y += 44;
        ui.text("Relics", Vector2{r.x + 24, y}, 22, Color{180, 180, 195, 255});
        y += 34;
        float ix = r.x + 40;
        for (const auto& id : p.relics) {
            if (const RelicDef* rd = findRelic(app_.data(), id)) ui.icon(Vector2{ix, y + 16}, 17, rd->color, rd->glyph.c_str());
            ix += 40;
            if (ix > r.x + cw - 30) {
                ix = r.x + 40;
                y += 40;
            }
        }
    }
    if (time_ > 1.f) {
        const float pulse = 0.6f + 0.4f * std::sin(time_ * 3.f);
        const Color c{255, 230, 170, static_cast<unsigned char>(255 * pulse)};
        drawPrompt(ui, eng::PadFamily::Xbox, Prompt::Confirm, "", Vector2{ui.width() * 0.5f - 180, ui.height() - 110}, 44);
        drawPrompt(ui, eng::PadFamily::Keyboard, Prompt::Confirm, "Back to the lobby", Vector2{ui.width() * 0.5f - 120, ui.height() - 110}, 44, c);
    }
}

}  // namespace game
