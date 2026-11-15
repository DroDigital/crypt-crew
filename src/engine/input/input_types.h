#pragma once
#include <raylib.h>

#include <cstdint>

namespace eng {

constexpr int kKeyboardDevice = 0;  // keyboard + mouse
constexpr int kMaxPads = 4;
constexpr int kMaxDevices = 1 + kMaxPads;  // device d>0 is gamepad index d-1
constexpr int kMaxSlots = 4;

enum class PadFamily : uint8_t { Keyboard, Xbox, PlayStation, Switch };

enum Button : uint32_t {
    kBtnAttack = 1u << 0,
    kBtnSpecial = 1u << 1,
    kBtnDodge = 1u << 2,
    kBtnInteract = 1u << 3,
    kBtnPause = 1u << 4,
    kBtnConfirm = 1u << 5,
    kBtnBack = 1u << 6,
    kBtnUp = 1u << 7,
    kBtnDown = 1u << 8,
    kBtnLeft = 1u << 9,
    kBtnRight = 1u << 10,
    kBtnJoin = 1u << 11,
};

struct RawDeviceState {
    bool connected = false;
    PadFamily family = PadFamily::Xbox;
    Vector2 move{0, 0};   // dead-zoned, length <= 1 (x right, y down)
    Vector2 aim{0, 0};    // right stick, dead-zoned (zero when idle)
    bool usesMouse = false;
    Vector2 mouse{0, 0};  // screen position (keyboard device only)
    uint32_t down = 0;    // Button bits held this frame
};

}  // namespace eng
