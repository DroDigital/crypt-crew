#include "game/scenes/gallery_scene.h"

#include <raymath.h>

namespace game {

namespace {
constexpr const char* kAtlas = "assets/kaykit_dungeon/textures/dungeon_texture.png";
}

GalleryScene::GalleryScene(App& app, bool iconMode) : app_(app), icon_(iconMode) {
    auto& data = app.data();
    float x = -7.5f;
    for (const auto& id : data.classOrder) {
        Entry e;
        e.visual.initHero(app.characters(), data.cls(id));
        e.pos = Vector3{x, 0.f, 2.5f};
        e.label = data.cls(id).name;
        entries_.push_back(std::move(e));
        x += 5.f;
    }
    x = -7.5f;
    for (const char* id : {"minion", "warrior", "rogue", "mage"}) {
        Entry e;
        e.visual.initEnemy(app.characters(), data.enemy(id));
        e.pos = Vector3{x, 0.f, -2.5f};
        e.label = data.enemy(id).name;
        entries_.push_back(std::move(e));
        x += 5.f;
    }
    if (icon_) {
        // App icon: the Knight alone, posed with the Cheer clip.
        entries_.resize(1);
        entries_[0].pos = Vector3{0, 0, 0};
        entries_[0].visual.play("cheer", true, 1.f, 0.f);
        entries_[0].visual.update(0.45f, nullptr);
    }
    // Stagger idle phases so the line-up doesn't move in lockstep.
    for (size_t i = 0; i < entries_.size(); ++i) entries_[i].visual.update(0.13f * static_cast<float>(i), nullptr);
    floor_ = app.assets().model("assets/kaykit_dungeon/models/floor_tile_large.glb", kAtlas);
    wall_ = app.assets().model("assets/kaykit_dungeon/models/wall.glb", kAtlas);
    torch_ = app.assets().model("assets/kaykit_dungeon/models/torch_mounted.glb", kAtlas);
    for (float tx : {-10.f, -2.f, 6.f})
        lights_.push_back(eng::PointLight{Vector3{tx + 2.f, 3.f, -5.4f}, Vector3{2.6f, 1.5f, 0.7f}, 11.f});
    lights_.push_back(eng::PointLight{Vector3{0.f, 4.f, 6.f}, Vector3{1.0f, 0.9f, 0.8f}, 16.f});
}

void GalleryScene::fixedUpdate(float dt) {
    time_ += dt;
    if (icon_) return;  // frozen pose
    for (auto& e : entries_) e.visual.update(dt, nullptr);
}

void GalleryScene::render(float /*alpha*/) {
    eng::Renderer& r = app_.renderer();
    Camera3D cam{};
    cam.position = Vector3{0.f, 11.f, 17.f};
    cam.target = Vector3{0.f, 1.2f, 0.f};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 40.f;
    cam.projection = CAMERA_PERSPECTIVE;
    if (icon_) {
        cam.position = Vector3{1.6f, 3.2f, 5.0f};
        cam.target = Vector3{0.f, 1.45f, 0.f};
        cam.fovy = 34.f;
    }
    eng::Lighting L;
    L.ambient = Vector3{0.42f, 0.46f, 0.58f};
    L.sunColor = Vector3{0.45f, 0.5f, 0.62f};
    L.fogColor = Vector3{0.04f, 0.05f, 0.08f};
    std::vector<eng::PointLight> lights = lights_;
    for (size_t i = 0; i < 3; ++i)
        lights[i].color = Vector3Scale(lights_[i].color, 0.9f + 0.1f * std::sin(time_ * 9.f + static_cast<float>(i) * 2.f));
    r.beginScene(cam, L, lights);
    for (int gx = -3; gx < 3; ++gx)
        for (int gz = -2; gz < 2; ++gz)
            if (floor_) r.drawModel(*floor_, MatrixTranslate(static_cast<float>(gx) * 4.f + 2.f, 0.f, static_cast<float>(gz) * 4.f + 2.f));
    for (int gx = -3; gx < 3; ++gx) {
        const float wx = static_cast<float>(gx) * 4.f + 2.f;
        if (wall_) r.drawModel(*wall_, MatrixTranslate(wx, 0.f, -8.f));
    }
    for (float tx : {-10.f, -2.f, 6.f})
        if (torch_) r.drawModel(*torch_, MatrixTranslate(tx + 2.f, 2.4f, -7.5f));
    for (auto& e : entries_) {
        e.visual.draw(r, e.pos, 0.f, 0.f, WHITE);
        r.shadow(e.pos, 0.9f, 0.55f);
    }
    r.flushDecals();
    r.endScene();
    r.present();
    if (icon_) return;
    for (auto& e : entries_) {
        Vector2 s = r.worldToScreen(Vector3Add(e.pos, Vector3{0, 0.f, 1.2f}));
        const int tw = MeasureText(e.label.c_str(), 20);
        DrawText(e.label.c_str(), static_cast<int>(s.x) - tw / 2, static_cast<int>(s.y) + 8, 20, RAYWHITE);
    }
    DrawText(TextFormat("Crypt Crew %s  gallery  %d fps  %d draws  MSAA %dx", CC_VERSION, GetFPS(), r.drawCalls(),
                        r.msaaSamples()),
             12, 10, 20, RAYWHITE);
}

}  // namespace game
