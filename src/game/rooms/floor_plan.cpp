#include "game/rooms/floor_plan.h"

namespace game {

int FloorPlan::mainLength() const {
    int n = 0;
    for (const auto& r : rooms) n += r.branch ? 0 : 1;
    return n;
}

}  // namespace game
