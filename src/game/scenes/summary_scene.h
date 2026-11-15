#pragma once
// Run summary: rooms cleared, gold earned, revives used, who died to what. Continue -> lobby.
#include "game/core/app.h"

namespace game {

class SummaryScene : public Scene {
public:
    explicit SummaryScene(App& app);
    void fixedUpdate(float dt) override { time_ += dt; }
    void frameUpdate(float realDt) override;
    void render(float alpha) override;
    const char* name() const override { return "summary"; }

private:
    App& app_;
    float time_ = 0.f;
};

}  // namespace game
