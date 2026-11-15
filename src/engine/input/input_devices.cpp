#include "engine/input/input_devices.h"

#include <cctype>
#include <cmath>
#include <string>

namespace eng {

Vector2 applyDeadZone(Vector2 v, float dz) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= dz || len < 1e-6f) return Vector2{0, 0};
    const float scaled = std::fmin((len - dz) / (1.f - dz), 1.f);
    return Vector2{v.x / len * scaled, v.y / len * scaled};
}

PadFamily detectPadFamily(const char* name) {
    if (!name) return PadFamily::Xbox;
    std::string n(name);
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
    if (has("playstation") || has("dualsense") || has("dualshock") || has("ps3") || has("ps4") || has("ps5") ||
        has("sony") || has("wireless controller"))
        return PadFamily::PlayStation;
    if (has("nintendo") || has("switch") || has("pro controller") || has("joy-con")) return PadFamily::Switch;
    return PadFamily::Xbox;
}

namespace {

uint32_t keyboardButtons() {
    uint32_t b = 0;
    auto k = [](int key) { return IsKeyDown(key); };
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || k(KEY_J)) b |= kBtnAttack;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || k(KEY_K)) b |= kBtnSpecial;
    if (k(KEY_SPACE) || k(KEY_LEFT_SHIFT)) b |= kBtnDodge;
    if (k(KEY_E)) b |= kBtnInteract;
    if (k(KEY_ESCAPE) || k(KEY_P)) b |= kBtnPause;
    if (k(KEY_ENTER) || k(KEY_KP_ENTER)) b |= kBtnConfirm | kBtnJoin;
    if (k(KEY_ESCAPE) || k(KEY_BACKSPACE)) b |= kBtnBack;
    if (k(KEY_UP) || k(KEY_W)) b |= kBtnUp;
    if (k(KEY_DOWN) || k(KEY_S)) b |= kBtnDown;
    if (k(KEY_LEFT) || k(KEY_A)) b |= kBtnLeft;
    if (k(KEY_RIGHT) || k(KEY_D)) b |= kBtnRight;
    return b;
}

uint32_t padButtons(int pad, Vector2 move, float triggerDz) {
    uint32_t b = 0;
    auto d = [&](int btn) { return IsGamepadButtonDown(pad, btn); };
    // Triggers: some mappings expose them as axes [-1..1], others as buttons.
    const float lt = GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_TRIGGER);
    const float rt = GetGamepadAxisMovement(pad, GAMEPAD_AXIS_RIGHT_TRIGGER);
    const bool ltDown = d(GAMEPAD_BUTTON_LEFT_TRIGGER_2) || (lt > -1.f + 2.f * triggerDz && lt != 0.f);
    const bool rtDown = d(GAMEPAD_BUTTON_RIGHT_TRIGGER_2) || (rt > -1.f + 2.f * triggerDz && rt != 0.f);
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) || rtDown) b |= kBtnAttack;
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_UP) || ltDown) b |= kBtnSpecial;
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || d(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)) b |= kBtnDodge;
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || d(GAMEPAD_BUTTON_LEFT_TRIGGER_1)) b |= kBtnInteract;
    if (d(GAMEPAD_BUTTON_MIDDLE_RIGHT)) b |= kBtnPause | kBtnJoin;
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) b |= kBtnConfirm | kBtnJoin;
    if (d(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || d(GAMEPAD_BUTTON_MIDDLE_LEFT)) b |= kBtnBack;
    if (d(GAMEPAD_BUTTON_LEFT_FACE_UP) || move.y < -0.6f) b |= kBtnUp;
    if (d(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || move.y > 0.6f) b |= kBtnDown;
    if (d(GAMEPAD_BUTTON_LEFT_FACE_LEFT) || move.x < -0.6f) b |= kBtnLeft;
    if (d(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || move.x > 0.6f) b |= kBtnRight;
    return b;
}

}  // namespace

void pollDevices(RawDeviceState out[kMaxDevices], const DeadZones& dz) {
    // Keyboard + mouse (always connected).
    RawDeviceState& kb = out[kKeyboardDevice];
    kb.connected = true;
    kb.family = PadFamily::Keyboard;
    Vector2 mv{0, 0};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) mv.y -= 1.f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) mv.y += 1.f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) mv.x -= 1.f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) mv.x += 1.f;
    const float l = std::sqrt(mv.x * mv.x + mv.y * mv.y);
    kb.move = l > 0.f ? Vector2{mv.x / l, mv.y / l} : Vector2{0, 0};
    kb.aim = Vector2{0, 0};
    kb.usesMouse = true;
    kb.mouse = GetMousePosition();
    kb.down = keyboardButtons();

    for (int p = 0; p < kMaxPads; ++p) {
        RawDeviceState& s = out[1 + p];
        s.connected = IsGamepadAvailable(p);
        s.usesMouse = false;
        if (!s.connected) {
            s.move = s.aim = Vector2{0, 0};
            s.down = 0;
            continue;
        }
        s.family = detectPadFamily(GetGamepadName(p));
        s.move = applyDeadZone(Vector2{GetGamepadAxisMovement(p, GAMEPAD_AXIS_LEFT_X), GetGamepadAxisMovement(p, GAMEPAD_AXIS_LEFT_Y)},
                               dz.move);
        s.aim = applyDeadZone(Vector2{GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_X), GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_Y)},
                              dz.aim);
        s.down = padButtons(p, s.move, dz.trigger);
    }
}

}  // namespace eng
