#pragma once

#include <cstdint>
#include <vector>

#include "my_hid_adapter.h"

// Data consumed by render side. It is intentionally flattened so render code
// does not need to know HID internals. Legacy fixed fields are kept for
// backward compatibility; dynamic vectors carry every auto-detected signal.
struct HidOverlayState {
    bool connected = false;

    // Legacy fixed fields
    bool button01 = false;
    bool button02 = false;
    bool button03 = false;
    bool button04 = false;
    bool button05 = false;
    bool button06 = false;
    bool button07 = false;
    float xNorm = 0.0f;
    int xDirection = 0;
    uint64_t tickMs = 0;

    // Dynamic arrays, parallel to the auto-detected config.buttons / config.axes.
    // Empty when no dynamic mapping is active (consumer should fall back to legacy fields).
    std::vector<bool> buttons;     // all button pressed states
    std::vector<float> axesNorm;   // all axis normalized [0,1]
    std::vector<LONG> axesRaw;     // all axis raw values
    std::vector<int> axesDir;      // all axis directions
};

class HidInputBackend {
public:
    explicit HidInputBackend(MyHidConfig cfg = {});
    ~HidInputBackend();

    // Start worker thread + hidden message window for Raw Input callbacks.
    bool start();

    // Stop worker thread and clear cached state.
    void stop();

    // Non-blocking snapshot read; returns false until at least one state is published.
    bool tryGetLatest(HidOverlayState& out) const;

    // Change target device at runtime; triggers re-scan.
    void setTargetVidPid(uint16_t vid, uint16_t pid);

    // Auto-detect a connected device by enumerating its capabilities and use them.
    // Returns the detected capability summary via outCaps (optional).
    bool autoConfigure(uint16_t vid, uint16_t pid, HidCapabilities* outCaps = nullptr);

    // Read the backend's current config (after autoConfigure / setTargetVidPid).
    MyHidConfig getConfig() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
