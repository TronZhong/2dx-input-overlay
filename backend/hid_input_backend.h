#pragma once

#include "core/types/hid_types.h"
#include "core/capabilities/hid_capabilities.h"

class HidInputBackend {
public:
    explicit HidInputBackend(MyHidConfig cfg = {});
    ~HidInputBackend();

    bool start();
    void stop();

    bool tryGetLatest(HidOverlayState& out) const;

    void setTargetVidPid(uint16_t vid, uint16_t pid);
    bool autoConfigure(uint16_t vid, uint16_t pid, HidCapabilities* outCaps = nullptr);
    MyHidConfig getConfig() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};