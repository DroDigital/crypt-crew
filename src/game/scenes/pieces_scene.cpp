#include "game/scenes/pieces_scene.h"

#include <cmath>
#include <raymath.h>

namespace game {

PiecesScene::PiecesScene(App& app, std::vector<std::string> names) : app_(app), names_(std::move(names)) {
    for (const auto& n : names_)
        models_.push_back(app.assets().model("assets/kaykit_dungeon/models/" + n + ".glb",
                                             "assets/kaykit_dungeon/textures/dungeon_texture.png"));
}

void PiecesScene::render(float) {
    eng::Renderer& r = app_.renderer();
    const int cols = 5;
    const float sp = 7.f;
    const int rows = (static_cast<int>(names_.size()) + cols - 1) / cols;
    Camera3D cam{};
    const float cz = static_cast<float>(rows - 1) * sp * 0.5f;
    cam.position = Vector3{0, 26.f + static_cast<float>(rows) * 3.f, cz + 26.f};
    cam.target = Vector3{0, 0, cz};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 45;
    eng::Lighting L;
    L.ambient = Vector3{0.5f, 0.5f, 0.55f};
    L.sunColor = Vector3{0.6f, 0.6f, 0.6f};
    L.fogStart = 200;
    L.fogEnd = 300;
    r.beginScene(cam, L, {});
    for (size_t i = 0; i < models_.size(); ++i) {
        const float x = (static_cast<float>(i % cols) - (cols - 1) * 0.5f) * sp;
        const float z = static_cast<float>(i / cols) * sp;
        if (models_[i]) r.drawModel(*models_[i], MatrixTranslate(x, 0, z));
    }
    r.flushDecals();
    r.endScene();
    r.present();
    for (size_t i = 0; i < models_.size(); ++i) {
        const float x = (static_cast<float>(i % cols) - (cols - 1) * 0.5f) * sp;
        const float z = static_cast<float>(i / cols) * sp;
        Vector2 s = r.worldToScreen(Vector3{x, 0, z + 2.5f});
        DrawText(names_[i].c_str(), static_cast<int>(s.x) - MeasureText(names_[i].c_str(), 18) / 2, static_cast<int>(s.y), 18, YELLOW);
    }
}

}  // namespace game
