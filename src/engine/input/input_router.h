#pragma once
// Assigns devices (keyboard+mouse, each gamepad) to player slots and exposes per-slot
// input. Pure logic: feed it RawDeviceState each frame (unit-testable without a window).
#include <array>
#include <cstddef>

#include "engine/input/input_types.h"

namespace eng {

struct SlotInput {
    Vector2 move{0, 0};
    Vector2 aimStick{0, 0};
    bool usesMouse = false;
    Vector2 mouse{0, 0};
    uint32_t held = 0;
};

class InputRouter {
public:
    // Call once per frame with the current device states.
    void update(const RawDeviceState devices[kMaxDevices]);

    // --- Slots ---
    // Returns the slot or -1 (already joined / full). `preferred` picks a specific free slot.
    int join(int device, int preferred = -1);
    void leave(int slot);
    void clearSlots();
    bool slotActive(int slot) const { return slot >= 0 && slot < kMaxSlots && slots_[slot].device >= 0; }
    int slotDevice(int slot) const { return slotActive(slot) ? slots_[slot].device : -1; }
    int slotOfDevice(int device) const;
    int activeSlots() const;
    bool slotDisconnected(int slot) const { return slotActive(slot) && slots_[slot].disconnected; }
    int firstDisconnectedSlot() const;
    PadFamily slotFamily(int slot) const;

    // --- Per-slot input ---
    const SlotInput& input(int slot) const { return slots_[slot].in; }
    // Edges latched since the last consume (for the fixed-step simulation).
    uint32_t consumePressed(int slot);
    // Edges this frame (for menus/UI, not cleared by consume).
    uint32_t framePressed(int slot) const { return slotActive(slot) ? slots_[slot].framePressed : 0; }
    void clearLatched();

    // --- Raw device edges (lobby join, menus before joining) ---
    uint32_t devicePressed(int device) const { return devices_[device].pressed; }
    bool deviceConnected(int device) const { return devices_[device].connected; }
    PadFamily deviceFamily(int device) const { return devices_[device].family; }
    // Any device pressed any of `mask` this frame; returns device or -1.
    int anyDevicePressed(uint32_t mask) const;

private:
    struct Device {
        bool connected = false;
        PadFamily family = PadFamily::Xbox;
        uint32_t down = 0, pressed = 0;
    };
    struct Slot {
        int device = -1;
        bool disconnected = false;
        SlotInput in;
        uint32_t latched = 0;
        uint32_t framePressed = 0;
    };
    std::array<Device, kMaxDevices> devices_{};
    std::array<Slot, kMaxSlots> slots_{};
};

}  // namespace eng
