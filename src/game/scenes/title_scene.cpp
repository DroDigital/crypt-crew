#include "game/scenes/title_scene.h"

#include <cmath>
#include <cstdio>
#include <raymath.h>

#include "game/scenes/lobby_scene.h"
#include "game/ui/glyphs.h"

namespace game {
namespace {
constexpr const char* kAtlas = "assets/kaykit_dungeon/textures/dungeon_texture.png";
const Vector3 kHeroPos[4] = {{-3.3f, 0, 0.6f}, {-1.1f, 0, -0.4f}, {1.1f, 0, -0.4f}, {3.3f, 0, 0.6f}};
const float kHeroYaw[4] = {0.45f, 0.15f, -0.15f, -0.45f};
}  // namespace

TitleScene::TitleScene(App& app) : app_(app) {
    for (size_t i = 0; i < heroes_.size(); ++i) {
        heroes_[i].initHero(app.characters(), app.data().cls(app.data().classOrder[i]));
        heroes_[i].update(0.27f * static_cast<float>(i), nullptr);
    }
    app_.input().clearSlots();
    app_.audio().playMusic("explore", 2.f);
}

void TitleScene::fixedUpdate(float dt) {
    time_ += dt;
    for (auto& h : heroes_) {
        h.update(dt, nullptr);
        if (h.finished()) h.play("idle");
    }
}

void TitleScene::frameUpdate(float) {
    eng::InputRouter& in = app_.input();
    uint32_t b = 0;
    for (int d = 0; d < eng::kMaxDevices; ++d)
        if (in.devicePressed(d)) {
            b |= in.devicePressed(d);
            lastDevice_ = d;
        }
    if (settings_.isOpen()) {
        if (settings_.update(b, app_.settings(), app_.audio())) {}
        return;
    }
    if (b & eng::kBtnUp) focus_ = (focus_ + 2) % 3, app_.audio().play("ui_move");
    if (b & eng::kBtnDown) focus_ = (focus_ + 1) % 3, app_.audio().play("ui_move");
    if (b & (eng::kBtnConfirm | eng::kBtnJoin)) {
        app_.audio().play("ui_confirm");
        if (focus_ == 0) app_.switchTo(std::make_unique<LobbyScene>(app_, std::max(app_.options().dummies, 0)));
        else if (focus_ == 1) settings_.open();
        else app_.quit();
        for (auto& h : heroes_) h.play("cheer", true);
    }
}

void TitleScene::render(float) {
    eng::Renderer& r = app_.renderer();
    const float sway = std::sin(time_ * 0.15f) * 0.6f;
    Camera3D cam{};
    cam.position = Vector3{sway * 2.f, 3.6f, 9.5f};
    cam.target = Vector3{0.f, 1.6f, 0.f};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 40.f;
    const FloorTheme& th = app_.data().floors[0];
    eng::Lighting L = th.lighting;
    L.fogStart = 14.f;
    L.fogEnd = 32.f;
    std::vector<eng::PointLight> lights;
    for (float x : {-6.f, 6.f}) {
        const float flick = 0.9f + 0.1f * std::sin(time_ * 11.f + x);
        lights.push_back(eng::PointLight{Vector3{x, 3.f, -2.6f}, Vector3Scale(th.torchColor, flick), 9.f});
    }
    lights.push_back(eng::PointLight{Vector3{0, 4.f, 4.f}, Vector3{0.9f, 0.85f, 0.8f}, 12.f});
    r.beginScene(cam, L, lights);
    eng::AssetCache& a = app_.assets();
    const auto* floor = a.model("assets/kaykit_dungeon/models/floor_tile_large.glb", kAtlas);
    const auto* wall = a.model("assets/kaykit_dungeon/models/wall.glb", kAtlas);
    const auto* torch = a.model("assets/kaykit_dungeon/models/torch_mounted.glb", kAtlas);
    const auto* banner = a.model("assets/kaykit_dungeon/models/banner_patternA_blue.glb", kAtlas);
    const auto* pillar = a.model("assets/kaykit_dungeon/models/pillar.glb", kAtlas);
    for (int x = -4; x < 4; ++x)
        for (int z = -2; z < 2; ++z) r.drawModel(*floor, MatrixTranslate(static_cast<float>(x) * 4.f + 2.f, 0, static_cast<float>(z) * 4.f + 2.f));
    for (int x = -4; x < 4; ++x) r.drawModel(*wall, MatrixTranslate(static_cast<float>(x) * 4.f + 2.f, 0, -4.f));
    for (float x : {-6.f, 6.f}) r.drawModel(*torch, MatrixTranslate(x, 2.4f, -3.5f));
    r.drawModel(*banner, MatrixTranslate(0, 0, -3.5f));
    for (float x : {-8.f, 8.f}) r.drawModel(*pillar, MatrixTranslate(x, 0, 0.5f));
    for (size_t i = 0; i < heroes_.size(); ++i) {
        heroes_[i].draw(r, kHeroPos[i], kHeroYaw[i], 0.f, WHITE);
        r.shadow(kHeroPos[i], 0.9f, 0.55f);
    }
    r.flushDecals();
    r.endScene();
    r.present();

    const eng::Ui& ui = app_.ui();
    ui.text("CRYPT CREW", Vector2{ui.width() * 0.5f, 70}, 128, Color{245, 225, 180, 255}, eng::Align::Center);
    ui.text("A couch co-op dungeon roguelite for 1-4 adventurers", Vector2{ui.width() * 0.5f, 210}, 30,
            Color{200, 195, 215, 255}, eng::Align::Center);
    const char* items[3] = {"Play", "Settings", "Quit"};
    const float bw = 380, bh = 72;
    for (int i = 0; i < 3; ++i)
        ui.button(items[i], Rectangle{(ui.width() - bw) * 0.5f, ui.height() * 0.62f + static_cast<float>(i) * (bh + 16), bw, bh},
                  focus_ == i, false);
    char buf[64];
    std::snprintf(buf, sizeof buf, "v%s", CC_VERSION);
    ui.text(buf, Vector2{ui.width() - 20, ui.height() - 40}, 24, Color{170, 170, 190, 255}, eng::Align::Right);
    ui.text("CC0 art by Kay Lousberg (KayKit)", Vector2{20, ui.height() - 40}, 20, Color{140, 140, 160, 255});
    const eng::PadFamily fam = app_.input().deviceFamily(lastDevice_);
    settings_.draw(ui, app_.settings(), fam);
}

}  // namespace game
