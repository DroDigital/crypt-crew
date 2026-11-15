#include "game/ui/relic_picker.h"

#include <algorithm>
#include <cmath>
#include <raymath.h>

#include "engine/core/log.h"
#include "game/data/relic_defs.h"
#include "game/player/player_system.h"
#include "game/ui/glyphs.h"

namespace game {

void grantRelic(World& w, Player& p, const std::string& relicId) {
    eng::log::info("P%d takes relic %s", p.slot + 1, relicId.c_str());
    auto& rec = w.svc.state.players[static_cast<size_t>(p.slot & 3)];
    rec.relics.push_back(relicId);
    applyPlayerStats(p, computeStats(w.svc.data, rec.relics));
    p.relicHooks = hooksOf(w.svc.data, rec.relics);
    w.emit("relic_glow", Vector3Add(p.k.pos, Vector3{0, 1.2f, 0}), Vector3{0, 1, 0}, 8.f);
    w.sound("relic");
    if (const RelicDef* r = findRelic(w.svc.data, relicId))
        w.text(Vector3Add(p.k.pos, Vector3{0, 3.f, 0}), r->name.c_str(), r->color, 26.f);
}

void RelicPicker::open(World& w) {
    for (Player& p : w.players) {
        if (p.state == PState::Dead) continue;
        SlotPick& s = slots_[static_cast<size_t>(p.slot & 3)];
        s = SlotPick{};
        s.options = rollRelicOffer(w.svc.data, p.cls->id, w.svc.state.players[static_cast<size_t>(p.slot & 3)].relics, w.rng, 3);
        s.open = !s.options.empty();
        s.timer = w.rng.uniform(0.8f, 1.6f);
    }
}

bool RelicPicker::active() const {
    for (const auto& s : slots_)
        if (s.open) return true;
    return false;
}

void RelicPicker::close() {
    for (auto& s : slots_) s.open = false;
}

void RelicPicker::choose(World& w, Player& p, const std::string& relicId) {
    grantRelic(w, p, relicId);
    slots_[static_cast<size_t>(p.slot & 3)].open = false;
}

void RelicPicker::update(World& w, eng::InputRouter& in, float realDt) {
    for (Player& p : w.players) {
        SlotPick& s = slots_[static_cast<size_t>(p.slot & 3)];
        if (!s.open) continue;
        s.appear = std::min(1.f, s.appear + realDt * 4.f);
        const int n = static_cast<int>(s.options.size());
        if (p.dummy || !in.slotActive(p.slot)) continue;
        const uint32_t b = in.framePressed(p.slot);
        if (b & eng::kBtnLeft) s.focus = (s.focus + n - 1) % n, w.sound("ui_move");
        if (b & eng::kBtnRight) s.focus = (s.focus + 1) % n, w.sound("ui_move");
        if (b & (eng::kBtnConfirm | eng::kBtnAttack)) choose(w, p, s.options[static_cast<size_t>(s.focus)]);
    }
}

void RelicPicker::updateBots(World& w, const eng::InputRouter& in, float dt) {
    for (Player& p : w.players) {
        SlotPick& s = slots_[static_cast<size_t>(p.slot & 3)];
        if (!s.open || !(p.dummy || !in.slotActive(p.slot))) continue;
        s.appear = std::min(1.f, s.appear + dt * 4.f);
        s.timer -= dt;
        if (s.timer <= 0.f) choose(w, p, s.options[static_cast<size_t>(w.rng.range(static_cast<int>(s.options.size())))]);
    }
}

void RelicPicker::draw(const eng::Ui& ui, const World& w, const eng::InputRouter& in) const {
    for (const Player& p : w.players) {
        const SlotPick& s = slots_[static_cast<size_t>(p.slot & 3)];
        if (!s.open) continue;
        const bool right = (p.slot & 1) != 0, bottom = p.slot >= 2;
        const float cw = 200, ch = 250, gap = 14;
        const float pw = cw * 3 + gap * 2 + 40, ph = ch + 110;
        const float x = right ? ui.width() - pw - 30 : 30;
        const float y = bottom ? ui.height() - ph - 160 : 160;
        const float k = s.appear;
        const Rectangle panel{x, y + (1.f - k) * 40.f, pw, ph};
        ui.panel(panel, slotColor(p.slot));
        char buf[64];
        std::snprintf(buf, sizeof buf, "P%d - choose a relic", p.slot + 1);
        ui.text(buf, Vector2{panel.x + 20, panel.y + 12}, 26, slotColor(p.slot));
        for (size_t i = 0; i < s.options.size(); ++i) {
            const RelicDef* r = findRelic(w.svc.data, s.options[i]);
            if (!r) continue;
            const Rectangle c{panel.x + 20 + static_cast<float>(i) * (cw + gap), panel.y + 52, cw, ch};
            const bool f = static_cast<int>(i) == s.focus;
            ui.panel(c, f ? Color{255, 225, 150, 255} : Color{120, 120, 140, 255});
            ui.icon(Vector2{c.x + cw * 0.5f, c.y + 56}, 34, r->color, r->glyph.c_str());
            ui.textFit(r->name.c_str(), Vector2{c.x + cw * 0.5f, c.y + 104}, 22, cw - 16, f ? Color{255, 235, 190, 255} : RAYWHITE,
                       eng::Align::Center);
            // Word-wrap the description into lines of ~22 characters.
            std::string d = r->description, line;
            float ly = c.y + 138;
            size_t pos = 0;
            while (pos < d.size()) {
                size_t next = d.find(' ', pos);
                if (next == std::string::npos) next = d.size();
                const std::string word = d.substr(pos, next - pos);
                if (!line.empty() && ui.measure((line + " " + word).c_str(), 17).x > cw - 20) {
                    ui.text(line.c_str(), Vector2{c.x + cw * 0.5f, ly}, 17, Color{200, 200, 215, 255}, eng::Align::Center);
                    ly += 22;
                    line.clear();
                }
                line += (line.empty() ? "" : " ") + word;
                pos = next + 1;
            }
            if (!line.empty()) ui.text(line.c_str(), Vector2{c.x + cw * 0.5f, ly}, 17, Color{200, 200, 215, 255}, eng::Align::Center);
        }
        const eng::PadFamily fam = p.dummy ? eng::PadFamily::Xbox : in.slotFamily(p.slot);
        drawPrompt(ui, fam, Prompt::Confirm, "Take", Vector2{panel.x + 24, panel.y + ph - 50}, 32);
        if (p.dummy) ui.text("(bot choosing...)", Vector2{panel.x + pw - 24, panel.y + ph - 46}, 20, GRAY, eng::Align::Right);
    }
}

}  // namespace game
