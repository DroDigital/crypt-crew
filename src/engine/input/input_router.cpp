#include "engine/input/input_router.h"

namespace eng {

void InputRouter::update(const RawDeviceState devs[kMaxDevices]) {
    for (int d = 0; d < kMaxDevices; ++d) {
        Device& dev = devices_[static_cast<std::size_t>(d)];
        const uint32_t down = devs[d].connected ? devs[d].down : 0u;
        dev.pressed = down & ~dev.down;
        dev.down = down;
        dev.connected = devs[d].connected;
        dev.family = devs[d].family;
    }
    for (auto& s : slots_) {
        s.framePressed = 0;
        if (s.device < 0) continue;
        const RawDeviceState& raw = devs[s.device];
        const Device& dev = devices_[static_cast<std::size_t>(s.device)];
        s.disconnected = !raw.connected;
        if (s.disconnected) {
            s.in = SlotInput{};
            continue;
        }
        s.in.move = raw.move;
        s.in.aimStick = raw.aim;
        s.in.usesMouse = raw.usesMouse;
        s.in.mouse = raw.mouse;
        s.in.held = dev.down;
        s.framePressed = dev.pressed;
        s.latched |= dev.pressed;
    }
}

int InputRouter::join(int device) {
    if (device < 0 || device >= kMaxDevices) return -1;
    if (slotOfDevice(device) >= 0) return -1;
    for (int i = 0; i < kMaxSlots; ++i) {
        Slot& s = slots_[static_cast<std::size_t>(i)];
        if (s.device >= 0) continue;
        s = Slot{};
        s.device = device;
        // Swallow the press that caused the join so it doesn't also act in-game.
        s.in.held = devices_[static_cast<std::size_t>(device)].down;
        return i;
    }
    return -1;
}

void InputRouter::leave(int slot) {
    if (slot < 0 || slot >= kMaxSlots) return;
    slots_[static_cast<std::size_t>(slot)] = Slot{};
}

void InputRouter::clearSlots() {
    for (auto& s : slots_) s = Slot{};
}

int InputRouter::slotOfDevice(int device) const {
    for (int i = 0; i < kMaxSlots; ++i)
        if (slots_[static_cast<std::size_t>(i)].device == device) return i;
    return -1;
}

int InputRouter::activeSlots() const {
    int n = 0;
    for (const auto& s : slots_) n += s.device >= 0 ? 1 : 0;
    return n;
}

int InputRouter::firstDisconnectedSlot() const {
    for (int i = 0; i < kMaxSlots; ++i)
        if (slotDisconnected(i)) return i;
    return -1;
}

PadFamily InputRouter::slotFamily(int slot) const {
    if (!slotActive(slot)) return PadFamily::Xbox;
    return devices_[static_cast<std::size_t>(slots_[static_cast<std::size_t>(slot)].device)].family;
}

uint32_t InputRouter::consumePressed(int slot) {
    if (!slotActive(slot)) return 0;
    const uint32_t v = slots_[static_cast<std::size_t>(slot)].latched;
    slots_[static_cast<std::size_t>(slot)].latched = 0;
    return v;
}

void InputRouter::clearLatched() {
    for (auto& s : slots_) s.latched = 0;
}

int InputRouter::anyDevicePressed(uint32_t mask) const {
    for (int d = 0; d < kMaxDevices; ++d)
        if (devices_[static_cast<std::size_t>(d)].pressed & mask) return d;
    return -1;
}

}  // namespace eng
