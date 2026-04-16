#pragma once

#include <cstdint>

#include "my_hid_adapter.h"

// Data consumed by render side (OBS source later). It is intentionally flattened so
// render code does not need to know HID internals.
struct HidOverlayState {
    bool connected = false;
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

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
