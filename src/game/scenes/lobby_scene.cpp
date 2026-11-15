#include "game/scenes/lobby_scene.h"

#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "game/scenes/run_scene.h"
#include "game/ui/glyphs.h"

namespace game {

std::unique_ptr<Scene> makeTitle(App& app) { return app.makeScene("title"); }

LobbyScene::LobbyScene(App& app, int preJoinedBots) : app_(app) {
    app_.input().clearSlots();
    for (auto& s : slots_) s.rt = LoadRenderTexture(360, 440);
    for (int i = 0; i < preJoinedBots; ++i) addBot();
    app_.audio().playMusic("explore", 1.5f);
}

LobbyScene::~LobbyScene() {
    for (auto& s : slots_)
        if (s.rt.id) UnloadRenderTexture(s.rt);
}

int LobbyScene::firstFreeClass() const {
    const int n = static_cast<int>(app_.data().classOrder.size());
    for (int c = 0; c < n; ++c) {
        bool taken = false;
        for (const auto& s : slots_) taken |= s.joined && s.classIdx == c;
        if (!taken) return c;
    }
    return 0;
}

void LobbyScene::setClass(int slot, int idx) {
    const int n = static_cast<int>(app_.data().classOrder.size());
    SlotUi& s = slots_[static_cast<size_t>(slot)];
    s.classIdx = ((idx % n) + n) % n;
    s.vis = CharacterVisual{};
    s.vis.initHero(app_.characters(), app_.data().cls(app_.data().classOrder[static_cast<size_t>(s.classIdx)]));
    s.vis.play("cheer", true, 1.f, 0.f);
    s.pop = 1.f;
}

void LobbyScene::addBot() {
    for (int i = 0; i < 4; ++i) {
        SlotUi& s = slots_[static_cast<size_t>(i)];
        if (s.joined || app_.input().slotActive(i)) continue;
        const int cls = firstFreeClass();
        s.joined = s.bot = s.ready = true;
        setClass(i, cls);
        return;
    }
}

void LobbyScene::startRun() {
    RunConfig rc;
    rc.seed = app_.options().seed ? app_.options().seed : static_cast<uint64_t>(GetTime() * 1e6) ^ 0xC0FFEEULL;
    for (int i = 0; i < 4; ++i) {
        const SlotUi& s = slots_[static_cast<size_t>(i)];
        if (!s.joined) continue;
        rc.players.push_back(PlayerSetup{i, app_.data().classOrder[static_cast<size_t>(s.classIdx)], s.bot});
        app_.state().players[static_cast<size_t>(i)].device = app_.input().slotDevice(i);
    }
    app_.audio().play("ui_confirm");
    app_.switchTo(std::make_unique<RunScene>(app_, rc));
}

void LobbyScene::fixedUpdate(float dt) {
    time_ += dt;
    for (auto& s : slots_) {
        if (!s.joined) continue;
        s.vis.update(dt, nullptr);
        if (s.vis.finished()) s.vis.play("idle");
        s.spin += dt * 0.6f;
        s.pop = std::max(0.f, s.pop - dt * 4.f);
    }
}

void LobbyScene::frameUpdate(float realDt) {
    eng::InputRouter& in = app_.input();
    eng::AudioManager& au = app_.audio();
    // Join from any unassigned device.
    const int dev = in.anyDevicePressed(eng::kBtnJoin);
    if (dev >= 0 && in.slotOfDevice(dev) < 0) {
        // Don't take a slot a bot already holds.
        int free = -1;
        for (int i = 0; i < 4 && free < 0; ++i)
            if (!slots_[static_cast<size_t>(i)].joined) free = i;
        if (free >= 0) {
            const int slot = in.join(dev, free);
            if (slot >= 0) {
                SlotUi& s = slots_[static_cast<size_t>(slot)];
                const int cls = firstFreeClass();
                s.joined = true;
                s.bot = false;
                s.ready = false;
                setClass(slot, cls);
                au.play("ui_confirm");
            }
        }
    }
    if (IsKeyPressed(KEY_F2)) {
        addBot();
        au.play("ui_confirm");
    }
    for (int i = 0; i < 4; ++i) {
        SlotUi& s = slots_[static_cast<size_t>(i)];
        if (!s.joined || s.bot) continue;
        if (!in.slotActive(i)) {  // device left
            s.joined = false;
            continue;
        }
        const uint32_t p = in.framePressed(i);
        if (!s.ready && (p & eng::kBtnLeft)) {
            setClass(i, s.classIdx - 1);
            au.play("ui_move");
        }
        if (!s.ready && (p & eng::kBtnRight)) {
            setClass(i, s.classIdx + 1);
            au.play("ui_move");
        }
        if (p & eng::kBtnConfirm) {
            s.ready = !s.ready;
            au.play(s.ready ? "ui_confirm" : "ui_back");
            if (s.ready) s.vis.play("cheer", true);
        } else if (p & eng::kBtnBack) {
            if (s.ready) s.ready = false;
            else {
                in.leave(i);
                s.joined = false;
            }
            au.play("ui_back");
        }
    }
    int joined = 0, ready = 0;
    for (const auto& s : slots_) {
        joined += s.joined ? 1 : 0;
        ready += (s.joined && s.ready) ? 1 : 0;
    }
    const bool allReady = joined > 0 && ready == joined;
    if (allReady) {
        if (countdown_ < 0.f) countdown_ = 2.f;
        countdown_ -= realDt;
        if (countdown_ <= 0.f) startRun();
    } else {
        countdown_ = -1.f;
    }
    // Back with nobody joined returns to the title screen.
    if (joined == 0) {
        const int d = in.anyDevicePressed(eng::kBtnBack);
        if (d >= 0 && in.slotOfDevice(d) < 0) {
            au.play("ui_back");
            app_.switchTo(makeTitle(app_));
        }
    }
}

void LobbyScene::render(float) {
    eng::Renderer& r = app_.renderer();
    const eng::Ui& ui = app_.ui();
    // Class previews into render textures.
    eng::Lighting L;
    L.ambient = Vector3{0.55f, 0.52f, 0.6f};
    L.sunColor = Vector3{0.75f, 0.7f, 0.65f};
    L.fogStart = 100.f;
    L.fogEnd = 200.f;
    const std::vector<eng::PointLight> lights{eng::PointLight{Vector3{1.5f, 3.f, 2.5f}, Vector3{1.6f, 1.0f, 0.6f}, 8.f}};
    Camera3D cam{};
    cam.position = Vector3{0.f, 1.9f, 5.2f};
    cam.target = Vector3{0.f, 1.15f, 0.f};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 34.f;
    for (int i = 0; i < 4; ++i) {
        SlotUi& s = slots_[static_cast<size_t>(i)];
        if (!s.joined) continue;
        r.beginPreview(s.rt, cam, L, lights, Color{0, 0, 0, 0});
        s.vis.draw(r, Vector3{0, 0, 0}, std::sin(s.spin) * 0.5f, 0.f, WHITE);
        r.endPreview();
    }
    // Background.
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{18, 16, 30, 255}, Color{6, 5, 10, 255});
    ui.text("CRYPT CREW", Vector2{ui.width() * 0.5f, 40}, 84, Color{240, 220, 180, 255}, eng::Align::Center);
    ui.text("Gather your crew", Vector2{ui.width() * 0.5f, 132}, 30, Color{180, 170, 200, 255}, eng::Align::Center);

    const float pw = 400, ph = 690, gap = 28;
    const float total = pw * 4 + gap * 3;
    const float x0 = (ui.width() - total) * 0.5f, y0 = 196;
    char buf[96];
    for (int i = 0; i < 4; ++i) {
        const SlotUi& s = slots_[static_cast<size_t>(i)];
        const Rectangle rr{x0 + static_cast<float>(i) * (pw + gap), y0, pw, ph};
        const Color sc = slotColor(i);
        ui.panel(rr, s.joined ? sc : Color{90, 90, 110, 255});
        std::snprintf(buf, sizeof buf, "P%d", i + 1);
        ui.text(buf, Vector2{rr.x + 20, rr.y + 14}, 34, sc);
        if (!s.joined) {
            const float pulse = 0.6f + 0.4f * std::sin(time_ * 3.f + static_cast<float>(i));
            const Color tc{200, 200, 220, static_cast<unsigned char>(255 * pulse)};
            ui.text("Press to join", Vector2{rr.x + pw * 0.5f, rr.y + ph * 0.42f}, 32, tc, eng::Align::Center);
            drawGlyph(ui, eng::PadFamily::Xbox, Prompt::Confirm, Vector2{rr.x + pw * 0.5f - 60, rr.y + ph * 0.52f}, 46);
            drawGlyph(ui, eng::PadFamily::Keyboard, Prompt::Confirm, Vector2{rr.x + pw * 0.5f + 60, rr.y + ph * 0.52f}, 46);
            continue;
        }
        const ClassDef& c = app_.data().cls(app_.data().classOrder[static_cast<size_t>(s.classIdx)]);
        const float k = 1.f + s.pop * 0.06f;
        const float tw = 360 * k, th = 440 * k;
        DrawTexturePro(s.rt.texture, Rectangle{0, 0, 360, -440},
                       Rectangle{(rr.x + (pw - tw) * 0.5f) * ui.scale(), (rr.y + 50) * ui.scale(), tw * ui.scale(), th * ui.scale()},
                       Vector2{0, 0}, 0.f, WHITE);
        const eng::PadFamily fam = s.bot ? eng::PadFamily::Xbox : app_.input().slotFamily(i);
        ui.text(c.name.c_str(), Vector2{rr.x + pw * 0.5f, rr.y + 488}, 44, c.color, eng::Align::Center);
        if (!s.ready && !s.bot) {
            ui.text("<", Vector2{rr.x + 30, rr.y + 488}, 44, RAYWHITE);
            ui.text(">", Vector2{rr.x + pw - 30, rr.y + 488}, 44, RAYWHITE, eng::Align::Right);
        }
        std::snprintf(buf, sizeof buf, "HP %d   Special: %.0fs", static_cast<int>(c.hp), c.specialCooldown);
        ui.text(buf, Vector2{rr.x + pw * 0.5f, rr.y + 542}, 22, Color{200, 200, 210, 255}, eng::Align::Center);
        // Wrap the description into two lines.
        std::string d = c.description;
        size_t cut = d.find(' ', d.size() / 2);
        if (cut == std::string::npos) cut = d.size();
        ui.textFit(d.substr(0, cut).c_str(), Vector2{rr.x + pw * 0.5f, rr.y + 574}, 18, pw - 30, Color{170, 170, 190, 255},
                   eng::Align::Center);
        if (cut < d.size())
            ui.textFit(d.substr(cut + 1).c_str(), Vector2{rr.x + pw * 0.5f, rr.y + 598}, 18, pw - 30, Color{170, 170, 190, 255},
                       eng::Align::Center);
        if (s.ready) {
            ui.text(s.bot ? "BOT READY" : "READY", Vector2{rr.x + pw * 0.5f, rr.y + 630}, 40, Color{120, 255, 140, 255}, eng::Align::Center);
        } else {
            drawPrompt(ui, fam, Prompt::Confirm, "Ready", Vector2{rr.x + 60, rr.y + 632}, 36);
            drawPrompt(ui, fam, Prompt::Back, "Leave", Vector2{rr.x + 230, rr.y + 632}, 36);
        }
    }
    if (countdown_ >= 0.f) {
        std::snprintf(buf, sizeof buf, "Descending in %d...", static_cast<int>(std::ceil(countdown_)));
        ui.text(buf, Vector2{ui.width() * 0.5f, ui.height() - 140}, 44, Color{255, 220, 140, 255}, eng::Align::Center);
    } else {
        ui.text("All joined players ready up to start.  F2: add AI teammate", Vector2{ui.width() * 0.5f, ui.height() - 120}, 24,
                Color{150, 150, 170, 255}, eng::Align::Center);
    }
    std::snprintf(buf, sizeof buf, "v%s", CC_VERSION);
    ui.text(buf, Vector2{ui.width() - 16, ui.height() - 36}, 20, Color{120, 120, 140, 255}, eng::Align::Right);
}

}  // namespace game
