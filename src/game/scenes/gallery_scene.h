#pragma once
// Model gallery: every hero and skeleton playing idle on a lit dungeon floor (M0 check).
#include <vector>

#include "game/core/app.h"

namespace game {

class GalleryScene : public Scene {
public:
    GalleryScene(App& app, bool iconMode = false);
    void fixedUpdate(float dt) override;
    void render(float alpha) override;
    const char* name() const override { return "gallery"; }

private:
    struct Entry {
        CharacterVisual visual;
        Vector3 pos;
        std::string label;
    };
    App& app_;
    std::vector<Entry> entries_;
    std::vector<eng::PointLight> lights_;
    const eng::ModelAsset* floor_ = nullptr;
    const eng::ModelAsset* wall_ = nullptr;
    const eng::ModelAsset* torch_ = nullptr;
    float time_ = 0.f;
    bool icon_ = false;
};

}  // namespace game
