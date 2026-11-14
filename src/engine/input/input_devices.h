#pragma once
// Reads raylib keyboard/mouse and per-index gamepads into RawDeviceState
// (dead zones, hot-plug, pad family detection for glyphs).
#include "engine/input/input_types.h"

namespace eng {

struct DeadZones {
    float move = 0.2f;
    float aim = 0.25f;
    float trigger = 0.3f;
};

void pollDevices(RawDeviceState out[kMaxDevices], const DeadZones& dz);
PadFamily detectPadFamily(const char* gamepadName);
// Radial dead zone with rescale so the response starts at 0 just past the dead zone.
Vector2 applyDeadZone(Vector2 v, float dz);

}  // namespace eng
