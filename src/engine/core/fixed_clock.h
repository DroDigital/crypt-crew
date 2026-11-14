#pragma once
// Fixed-step simulation clock. Hitstop and slow motion scale the *simulation* time
// that is fed into the accumulator; the frame rate is never touched.
#include <algorithm>

namespace eng {

class FixedClock {
public:
    explicit FixedClock(double hz = 60.0) : step_(1.0 / hz) {}

    // Feed real frame time; returns how many fixed steps to run this frame.
    int advance(double realDt) {
        realDt = std::min(realDt, 0.25);  // avoid spiral of death after stalls
        double scale = timeScale_;
        if (hitstopLeft_ > 0.0) {
            const double used = std::min(hitstopLeft_, realDt);
            hitstopLeft_ -= used;
            // During hitstop the sim is frozen; the rest of the frame runs at timeScale.
            accumulator_ += (realDt - used) * scale;
        } else {
            accumulator_ += realDt * scale;
        }
        int steps = 0;
        while (accumulator_ >= step_ && steps < 8) {
            accumulator_ -= step_;
            ++steps;
        }
        if (steps == 8) accumulator_ = 0.0;
        return steps;
    }
    double step() const { return step_; }
    float stepF() const { return static_cast<float>(step_); }
    // Interpolation factor between the previous and current sim state.
    float alpha() const { return static_cast<float>(accumulator_ / step_); }
    void hitstop(double seconds) { hitstopLeft_ = std::max(hitstopLeft_, seconds); }
    bool inHitstop() const { return hitstopLeft_ > 0.0; }
    void setTimeScale(double s) { timeScale_ = s; }
    double timeScale() const { return timeScale_; }
    void reset() {
        accumulator_ = 0.0;
        hitstopLeft_ = 0.0;
    }

private:
    double step_;
    double accumulator_ = 0.0;
    double timeScale_ = 1.0;
    double hitstopLeft_ = 0.0;
};

}  // namespace eng
