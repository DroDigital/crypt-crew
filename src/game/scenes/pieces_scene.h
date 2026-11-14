#pragma once
// Capture-only scene: a labeled grid of dungeon pieces, to choose room pieces by eye.
#include <string>
#include <vector>

#include "game/core/app.h"

namespace game {

class PiecesScene : public Scene {
public:
    PiecesScene(App& app, std::vector<std::string> names);
    void fixedUpdate(float) override {}
    void render(float alpha) override;
    const char* name() const override { return "pieces"; }

private:
    App& app_;
    std::vector<std::string> names_;
    std::vector<const eng::ModelAsset*> models_;
};

}  // namespace game
