#include "game/rooms/shrine.h"

#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "engine/core/log.h"
#include "game/data/relic_defs.h"
#include "game/ui/glyphs.h"
#include "game/ui/relic_picker.h"
#include "game/world/combat.h"

namespace game {
namespace {
constexpr const char* kAtlas = "assets/kaykit_dungeon/textures/dungeon_texture.png";
}

void Shrine::enter(World& w, const RoomTemplate& tpl) {
    active_ = true;
    cfg_ = eng::loadJsonFile("data/shrine.json");
    hasStatue_ = hasMerchant_ = false;
    shops_ = {};
    for (const TileMarker& m : tpl.markers) {
        if (m.kind == "statue") {
            statuePos_ = w.room.tileCenter(m.tx, m.tz);
            hasStatue_ = true;
        } else if (m.kind == "merchant") {
            merchantPos_ = w.room.tileCenter(m.tx, m.tz);
            hasMerchant_ = true;
        }
    }
    if (hasMerchant_) {
        const auto& mc = cfg_.at("merchant");
        merchant_ = CharacterVisual{};
        merchant_.initModel(w.svc.chars, mc.at("anim_map").get<std::string>(), mc.at("hide_meshes").get<std::vector<std::string>>());
        // Block the merchant and his table for movement.
        w.svc.phys.addStaticBox(Vector3{merchantPos_.x, 1.f, merchantPos_.z + 0.5f}, Vector3{1.1f, 1.f, 1.6f});
        w.room.nav.blockBox(Vector3{merchantPos_.x, 0, merchantPos_.z + 0.5f}, Vector3{1.1f, 0, 1.6f}, kNavInflate);
        // Stock: a heal plus a few relics, pricier on deeper floors.
        stock_.clear();
        stock_.push_back(Item{true, "", mc.at("heal").at("price").get<int>(), false});
        const float mult = 1.f + mc.value("relic_price_mult_per_floor", 0.25f) * static_cast<float>(w.svc.state.floor);
        std::vector<std::string> all;
        for (const auto& [id, r] : w.svc.data.relics) all.push_back(id);
        for (int i = 0; i < mc.value("relics", 3) && !all.empty(); ++i) {
            const size_t k = static_cast<size_t>(w.rng.range(static_cast<int>(all.size())));
            const RelicDef* r = findRelic(w.svc.data, all[k]);
            stock_.push_back(Item{false, all[k], static_cast<int>(std::lround(static_cast<float>(r->price) * mult)), false});
            all.erase(all.begin() + static_cast<long>(k));
        }
    }
    if (hasStatue_) {
        w.svc.phys.addStaticBox(Vector3{statuePos_.x, 2.f, statuePos_.z}, Vector3{0.75f, 2.f, 0.75f});
        w.room.nav.blockBox(statuePos_, Vector3{0.75f, 0, 0.75f}, kNavInflate);
        w.room.lights.push_back(eng::PointLight{Vector3Add(statuePos_, Vector3{0, 4.5f, 1.2f}), Vector3{2.2f, 1.8f, 0.9f}, 9.f});
    }
    if (hasMerchant_)
        w.room.lights.push_back(eng::PointLight{Vector3Add(merchantPos_, Vector3{0, 3.5f, 1.8f}), Vector3{1.6f, 1.2f, 0.7f}, 8.f});
    w.svc.phys.finalizeStatic();
}

void Shrine::buy(World& w, Player& p, Item& item) {
    if (item.sold || !w.svc.state.canAfford(item.price)) {
        w.sound("ui_back");
        return;
    }
    if (!item.heal) {
        const RelicDef* r = findRelic(w.svc.data, item.relic);
        if (!r || (!r->classOnly.empty() && r->classOnly != p.cls->id)) {
            w.sound("ui_back");
            return;
        }
    }
    w.svc.state.spend(item.price);
    char buf[32];
    std::snprintf(buf, sizeof buf, "-%d GOLD", item.price);
    w.text(Vector3Add(p.k.pos, Vector3{0, 3.4f, 0}), buf, Color{255, 210, 70, 255}, 24.f);
    merchant_.play("interact", true);
    if (item.heal) {
        healPlayer(w, p, p.hp.max * cfg_.at("merchant").at("heal").value("heal_fraction", 0.6f));
        w.emit("heal", Vector3Add(p.k.pos, Vector3{0, 1.f, 0}), Vector3{0, 1, 0});
        w.sound("revive", 0.6f, 1.3f);
    } else {
        grantRelic(w, p, item.relic);
        item.sold = true;
    }
}

void Shrine::fixedUpdate(World& w, const std::array<PlayerCommand, 4>& cmds, float dt) {
    if (!active_) return;
    statueGlow_ += dt;
    if (hasMerchant_) {
        merchant_.update(dt, nullptr);
        if (merchant_.finished()) merchant_.play("idle");
    }
    const float statueR = cfg_.at("statue").value("interact_radius", 3.f);
    const float merchR = cfg_.at("merchant").value("interact_radius", 3.2f) + 1.f;
    for (Player& p : w.players) {
        if (!p.canAct()) continue;
        const PlayerCommand& c = cmds[static_cast<size_t>(p.slot & 3)];
        Shop& shop = shops_[static_cast<size_t>(p.slot & 3)];
        if (shop.open && Vector3Distance(p.k.pos, merchantPos_) > merchR + 1.5f) shop.open = false;
        if (!(c.pressed & eng::kBtnInteract)) continue;
        if (hasStatue_ && !w.svc.state.statueUsed && Vector3Distance(p.k.pos, statuePos_) < statueR) {
            // The statue restores the whole team once per floor (downed teammates stand up).
            w.svc.state.statueUsed = true;
            const float frac = cfg_.at("statue").value("heal_fraction", 1.f);
            for (Player& o : w.players) {
                if (o.state == PState::Dead) continue;
                if (o.state == PState::Downed) {
                    o.state = PState::Revived;
                    o.stateTime = 0.f;
                    o.vis.play("revive", true);
                }
                o.hp.hp = std::max(o.hp.hp, o.hp.max * frac);
                w.emit("heal", Vector3Add(o.k.pos, Vector3{0, 1.f, 0}), Vector3{0, 1, 0}, 2.f);
            }
            w.emit("revive", Vector3Add(statuePos_, Vector3{0, 3.f, 0}), Vector3{0, 1, 0}, 2.f);
            w.sound("revive");
            w.text(Vector3Add(statuePos_, Vector3{0, 5.f, 0}), "THE CREW IS RESTORED", Color{255, 230, 140, 255}, 30.f);
        } else if (hasMerchant_ && Vector3Distance(p.k.pos, merchantPos_) < merchR) {
            shop.open = !shop.open;
            shop.justOpened = shop.open;
            shop.focus = 0;
            w.sound(shop.open ? "ui_confirm" : "ui_back");
        }
    }
}

void Shrine::frameUpdate(World& w, eng::InputRouter& in) {
    if (!active_) return;
    for (Player& p : w.players) {
        Shop& shop = shops_[static_cast<size_t>(p.slot & 3)];
        if (!shop.open || p.dummy) continue;
        if (shop.justOpened) {
            shop.justOpened = false;
            continue;
        }
        const uint32_t b = in.framePressed(p.slot);
        const int n = static_cast<int>(stock_.size());
        if (b & eng::kBtnUp) shop.focus = (shop.focus + n - 1) % n, w.sound("ui_move");
        if (b & eng::kBtnDown) shop.focus = (shop.focus + 1) % n, w.sound("ui_move");
        if (b & (eng::kBtnConfirm | eng::kBtnAttack)) buy(w, p, stock_[static_cast<size_t>(shop.focus)]);
        if (b & eng::kBtnBack) {
            shop.open = false;
            w.sound("ui_back");
        }
    }
}

void Shrine::render(eng::Renderer& r, World& w, float) {
    if (!active_) return;
    if (hasStatue_)
        if (const eng::ModelAsset* m = w.svc.assets.model("assets/kaykit_dungeon/models/" +
                                                              cfg_.at("statue").value("model", std::string("pillar_decorated")) + ".glb",
                                                          kAtlas)) {
            const float glow = w.svc.state.statueUsed ? 0.f : 0.25f + 0.15f * std::sin(statueGlow_ * 2.5f);
            r.drawModel(*m, MatrixTranslate(statuePos_.x, 0, statuePos_.z),
                        eng::DrawParams{nullptr, 0, Color{255, 235, 190, 255}, 0.f, Vector3{glow, glow * 0.8f, glow * 0.3f}, 0});
            if (!w.svc.state.statueUsed) r.decal(statuePos_, 3.f, Color{255, 220, 120, 100}, eng::DecalShape::Ring);
        }
    if (hasMerchant_) {
        merchant_.draw(r, merchantPos_, 0.f, 0.f, WHITE);
        r.shadow(merchantPos_, 0.9f, 0.5f);
        if (const eng::ModelAsset* t = w.svc.assets.model("assets/kaykit_dungeon/models/table_medium_decorated_A.glb", kAtlas))
            r.drawModel(*t, MatrixTranslate(merchantPos_.x, 0, merchantPos_.z + 1.6f));
    }
}

void Shrine::drawUi(const eng::Ui& ui, const eng::Renderer& r, const World& w, const eng::InputRouter& in) const {
    if (!active_) return;
    // World prompts.
    for (const Player& p : w.players) {
        if (!p.canAct() || p.dummy) continue;
        const eng::PadFamily fam = in.slotFamily(p.slot);
        if (hasStatue_ && !w.svc.state.statueUsed && Vector3Distance(p.k.pos, statuePos_) < 3.f) {
            const Vector2 s = ui.fromScreen(r.worldToScreen(Vector3Add(statuePos_, Vector3{0, 4.8f, 0})));
            drawPrompt(ui, fam, Prompt::Interact, "Pray: heal the crew (once)", Vector2{s.x - 170, s.y}, 32);
        }
        if (hasMerchant_ && !shops_[static_cast<size_t>(p.slot & 3)].open && Vector3Distance(p.k.pos, merchantPos_) < 3.2f) {
            const Vector2 s = ui.fromScreen(r.worldToScreen(Vector3Add(merchantPos_, Vector3{0, 3.2f, 0})));
            drawPrompt(ui, fam, Prompt::Interact, "Trade", Vector2{s.x - 60, s.y}, 32);
        }
    }
    // Shop panels, one per shopping player.
    char buf[96];
    for (const Player& p : w.players) {
        const Shop& shop = shops_[static_cast<size_t>(p.slot & 3)];
        if (!shop.open) continue;
        const bool right = (p.slot & 1) != 0, bottom = p.slot >= 2;
        const float pw = 520, rowH = 64;
        const float ph = 70 + rowH * static_cast<float>(stock_.size()) + 60;
        const Rectangle panel{right ? ui.width() - pw - 30 : 30, bottom ? ui.height() - ph - 160 : 160, pw, ph};
        ui.panel(panel, slotColor(p.slot));
        std::snprintf(buf, sizeof buf, "P%d - Merchant      Purse: %d", p.slot + 1, w.svc.state.purse);
        ui.text(buf, Vector2{panel.x + 20, panel.y + 14}, 24, slotColor(p.slot));
        for (size_t i = 0; i < stock_.size(); ++i) {
            const Item& it = stock_[i];
            const Rectangle row{panel.x + 16, panel.y + 56 + static_cast<float>(i) * rowH, pw - 32, rowH - 8};
            const bool f = static_cast<int>(i) == shop.focus;
            ui.roundRect(row, 0.2f, f ? Color{90, 75, 40, 230} : Color{30, 28, 40, 200});
            std::string name, desc;
            Color ic{120, 255, 140, 255};
            std::string glyph = "+";
            bool usable = true;
            if (it.heal) {
                name = cfg_.at("merchant").at("heal").value("name", std::string("Healing Draught"));
                desc = cfg_.at("merchant").at("heal").value("description", std::string(""));
            } else if (const RelicDef* rd = findRelic(w.svc.data, it.relic)) {
                name = rd->name;
                desc = rd->description;
                ic = rd->color;
                glyph = rd->glyph;
                usable = rd->classOnly.empty() || rd->classOnly == p.cls->id;
            }
            ui.icon(Vector2{row.x + 30, row.y + row.height * 0.5f}, 22, it.sold ? GRAY : ic, glyph.c_str());
            ui.textFit(name.c_str(), Vector2{row.x + 64, row.y + 4}, 22, row.width - 150, it.sold || !usable ? GRAY : RAYWHITE);
            ui.textFit(it.sold ? "Sold" : desc.c_str(), Vector2{row.x + 64, row.y + 30}, 16, row.width - 150, Color{180, 180, 195, 255});
            std::snprintf(buf, sizeof buf, "%dg", it.price);
            ui.text(buf, Vector2{row.x + row.width - 12, row.y + 14}, 24,
                    w.svc.state.canAfford(it.price) ? Color{255, 215, 80, 255} : Color{200, 90, 70, 255}, eng::Align::Right);
        }
        const eng::PadFamily fam = in.slotFamily(p.slot);
        drawPrompt(ui, fam, Prompt::Confirm, "Buy", Vector2{panel.x + 24, panel.y + ph - 48}, 32);
        drawPrompt(ui, fam, Prompt::Back, "Close", Vector2{panel.x + 200, panel.y + ph - 48}, 32);
    }
}

}  // namespace game
