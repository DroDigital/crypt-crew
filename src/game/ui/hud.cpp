#include "game/ui/hud.h"

#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "game/data/relic_defs.h"
#include "game/player/player_system.h"
#include "game/ui/glyphs.h"

namespace game {

void drawFloatingTexts(const eng::Ui& ui, const eng::Renderer& r, const World& w) {
    for (const FloatText& t : w.texts) {
        const Vector2 s = ui.fromScreen(r.worldToScreen(t.pos));
        const float k = t.life / t.maxLife;
        const float pop = k > 0.85f ? 1.f + (k - 0.85f) * 3.f : 1.f;
        Color c = t.color;
        c.a = static_cast<unsigned char>(255.f * std::min(1.f, k * 2.f));
        ui.text(t.text, Vector2{s.x, s.y}, t.size * pop, c, eng::Align::Center);
    }
}

namespace {

void playerPanel(const eng::Ui& ui, const World& w, const Player& p, const eng::InputRouter& input, Rectangle r, bool right) {
    const Color sc = slotColor(p.slot);
    ui.panel(r, Color{sc.r, sc.g, sc.b, 255});
    const float pad = 14.f;
    char buf[64];
    std::snprintf(buf, sizeof buf, "P%d  %s%s", p.slot + 1, p.cls->name.c_str(), p.dummy ? " (bot)" : "");
    ui.text(buf, Vector2{right ? r.x + r.width - pad : r.x + pad, r.y + 8}, 26, sc, right ? eng::Align::Right : eng::Align::Left);
    // HP bar.
    const Rectangle hb{r.x + pad, r.y + 44, r.width - pad * 2 - 64, 24};
    const Rectangle hbr = right ? Rectangle{r.x + pad + 64, hb.y, hb.width, hb.height} : hb;
    Color hpc = p.hp.hp / p.hp.max > 0.35f ? Color{90, 210, 90, 255} : Color{230, 70, 60, 255};
    if (p.state == PState::Downed) hpc = Color{140, 60, 60, 255};
    ui.bar(hbr, p.hp.hp / p.hp.max, hpc, Color{30, 20, 20, 220}, Color{255, 255, 255, 200}, p.hp.flash > 0 ? 1.f : 0.f);
    if (p.shield > 0.f)
        ui.bar(Rectangle{hbr.x, hbr.y + hbr.height - 6, hbr.width * std::min(1.f, p.shield / p.hp.max), 6}, 1.f,
               Color{140, 190, 255, 255}, BLANK);
    std::snprintf(buf, sizeof buf, "%d / %d", static_cast<int>(std::ceil(std::max(p.hp.hp, 0.f))), static_cast<int>(p.hp.max));
    ui.text(buf, Vector2{hbr.x + hbr.width * 0.5f, hbr.y + 1}, 20, RAYWHITE, eng::Align::Center);
    // Special cooldown dial.
    const Vector2 dial{right ? r.x + pad + 28 : r.x + r.width - pad - 28, r.y + 56};
    const float cd = p.cls->specialCooldown * p.stats.specialCdMult;
    const bool ready = p.specialCd <= 0.f;
    ui.icon(dial, 26, ready ? Color{70, 60, 30, 255} : Color{30, 30, 36, 255}, "");
    ui.ring(dial, 26, ready ? 1.f : 1.f - p.specialCd / std::max(cd, 0.01f), ready ? Color{255, 210, 80, 255} : Color{140, 140, 160, 255});
    drawGlyph(ui, input.slotFamily(p.slot), Prompt::Special, dial, 26);
    // Relic icons.
    float x = right ? r.x + r.width - pad - 18 : r.x + pad + 18;
    const float y = r.y + 98;
    for (const auto& id : w.svc.state.players[static_cast<size_t>(p.slot)].relics) {
        const RelicDef* def = findRelic(w.svc.data, id);
        if (!def) continue;
        ui.icon(Vector2{x, y}, 16, def->color, def->glyph.c_str());
        x += right ? -36.f : 36.f;
    }
    // Status line.
    if (p.state == PState::Downed) {
        std::snprintf(buf, sizeof buf, "DOWNED  %.0fs", std::ceil(std::max(p.downedTime, 0.f)));
        ui.text(buf, Vector2{r.x + r.width * 0.5f, r.y + 82}, 22, Color{255, 110, 100, 255}, eng::Align::Center);
    } else if (p.state == PState::Dead) {
        ui.text("FALLEN", Vector2{r.x + r.width * 0.5f, r.y + 82}, 22, Color{180, 60, 60, 255}, eng::Align::Center);
    } else if (p.rage > 0.f) {
        ui.text("RAGE", Vector2{r.x + r.width * 0.5f, r.y + 82}, 20, Color{255, 100, 70, 255}, eng::Align::Center);
    } else if (p.critWindow > 0.f) {
        ui.text("CRIT READY", Vector2{r.x + r.width * 0.5f, r.y + 82}, 20, Color{255, 220, 70, 255}, eng::Align::Center);
    }
}

}  // namespace

void drawBossBar(const eng::Ui& ui, const char* name, float frac, float flash) {
    const float w = std::min(1100.f, ui.width() * 0.6f);
    const Rectangle r{(ui.width() - w) * 0.5f, 92, w, 30};
    ui.text(name, Vector2{ui.width() * 0.5f, 54}, 34, Color{240, 210, 200, 255}, eng::Align::Center);
    ui.bar(r, frac, Color{200, 40, 50, 255}, Color{30, 10, 10, 230}, Color{255, 240, 240, 255}, flash > 0 ? frac + 0.01f : 0.f);
}

void drawHud(const eng::Ui& ui, const eng::Renderer& r, const World& w, const eng::InputRouter& input, const HudInfo& info) {
    (void)r;
    // Players in the four corners.
    const float pw = 400, ph = 124, m = 18;
    for (const Player& p : w.players) {
        const bool right = p.slot == 1 || p.slot == 3;
        const bool bottom = p.slot >= 2;
        const Rectangle rect{right ? ui.width() - pw - m : m, bottom ? ui.height() - ph - m : m, pw, ph};
        playerPanel(ui, w, p, input, rect, right);
    }
    // Shared purse, top-center.
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d", w.svc.state.purse);
    const float gs = 40.f * (1.f + 0.25f * info.goldPulse);
    const Vector2 gp{ui.width() * 0.5f, 14};
    ui.icon(Vector2{gp.x - ui.measure(buf, gs).x * 0.5f - 26, gp.y + gs * 0.55f}, 16, Color{250, 200, 50, 255}, "G",
            Color{120, 80, 10, 255});
    ui.text(buf, Vector2{gp.x + 6, gp.y}, gs, Color{255, 215, 80, 255}, eng::Align::Center);
    if (info.room > 0)
        std::snprintf(buf, sizeof buf, "Floor %d - %s   Room %d/%d  %s", info.floor + 1, info.floorName, info.room, info.rooms,
                      info.roomType);
    else
        std::snprintf(buf, sizeof buf, "Floor %d - %s   Side room  %s", info.floor + 1, info.floorName, info.roomType);
    ui.text(buf, Vector2{ui.width() * 0.5f, 62}, 22, Color{200, 200, 215, 230}, eng::Align::Center);

    // Revive prompts over downed players.
    const int cost = currentReviveCost(w);
    for (const Player& d : w.players) {
        if (d.state != PState::Downed) continue;
        const Vector2 s = ui.fromScreen(r.worldToScreen(Vector3Add(d.k.lerpPos(1.f), Vector3{0, 2.6f, 0})));
        if (d.reviveProgress > 0.f) ui.bar(Rectangle{s.x - 60, s.y + 30, 120, 12}, d.reviveProgress, Color{120, 255, 140, 255}, Color{0, 0, 0, 180});
        if (d.reviveBlockedNoGold) {
            std::snprintf(buf, sizeof buf, "Need %d gold", cost);
            ui.text(buf, s, 24, Color{255, 120, 90, 255}, eng::Align::Center);
            continue;
        }
        // Show the prompt with the glyph of the nearest standing teammate.
        const Player* helper = nullptr;
        float best = 1e9f;
        for (const Player& o : w.players) {
            if (!o.canAct()) continue;
            const float dd = Vector3Distance(o.k.pos, d.k.pos);
            if (dd < best) {
                best = dd;
                helper = &o;
            }
        }
        if (!helper) continue;
        std::snprintf(buf, sizeof buf, "Revive (%dg)", cost);
        const float tw = ui.measure(buf, 24 * 0.75f).x + 40;
        drawPrompt(ui, input.slotFamily(helper->slot), Prompt::Interact, buf, Vector2{s.x - tw * 0.5f, s.y}, 30,
                   w.svc.state.canAfford(cost) ? RAYWHITE : Color{255, 120, 90, 255});
    }
    if (info.banner && info.bannerAlpha > 0.f) {
        const unsigned char a = static_cast<unsigned char>(255.f * std::min(1.f, info.bannerAlpha));
        ui.text(info.banner, Vector2{ui.width() * 0.5f, ui.height() * 0.3f}, 72, Color{255, 230, 170, a}, eng::Align::Center);
    }
}

}  // namespace game
