#include "my_hid_adapter.h"

#include "adapter/button_parser.h"
#include "adapter/axis_parser.h"
#include "adapter/config_mapper.h"

#include <algorithm>
#include <windows.h>

namespace {
uint64_t currentTickMs() {
#if (_WIN32_WINNT >= 0x0600)
    return GetTickCount64();
#else
    return static_cast<uint64_t>(GetTickCount());
#endif
}
} // namespace

bool MyHidAdapter::matches(uint16_t vid, uint16_t pid) const {
    return cfg_.matches(vid, pid);
}

bool MyHidAdapter::updateFromReport(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    MyHidState& outState) const {

    if (!preparsed || !rawData || rawSize == 0) return false;
    outState.connected = true;

    // Buttons
    bool useDynamicButtons = !cfg_.buttons.empty();
    if (useDynamicButtons) {
        button_parser::parseButtons(preparsed, rawData, rawSize, cfg_, outState);
    } else {
        // No dynamic mapping - this shouldn't happen in new architecture
        outState.dynamicButtons.clear();
    }

    // Axes
    bool useDynamicAxes = !cfg_.axes.empty();
    if (useDynamicAxes) {
        axis_parser::parseAxes(preparsed, rawData, rawSize, cfg_, outState, axisTracks_);
    } else {
        outState.dynamicAxesNorm.clear();
        outState.dynamicAxesRaw.clear();
        outState.dynamicAxesDir.clear();
    }

    return true;
}

bool MyHidAdapter::autoDetect(const HidCapabilities& caps) {
    return config_mapper::mapCapabilitiesToConfig(caps, cfg_);
}

float MyHidAdapter::normalizeToUnit(LONG value, LONG logicalMin, LONG logicalMax) {
    if (logicalMax <= logicalMin) return 0.0f;
    float n = static_cast<float>(value - logicalMin) / static_cast<float>(logicalMax - logicalMin);
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    return n;
}

int MyHidAdapter::trackAxisDirection(size_t index, LONG raw, uint64_t now) const {
    if (index >= axisTracks_.size()) {
        axisTracks_.resize(index + 1);
    }
    AxisTrack& t = axisTracks_[index];
    LONG delta = 0;
    if (!t.hasPrev) {
        t.hasPrev = true;
    } else {
        delta = raw - t.prevRaw;
        if (delta > 128) delta -= 256;
        else if (delta < -128) delta += 256;
    }
    t.prevRaw = raw;

    if (delta > 0) {
        t.lastDir = 1;
        t.lastMoveTickMs = now;
        return 1;
    }
    if (delta < 0) {
        t.lastDir = -1;
        t.lastMoveTickMs = now;
        return -1;
    }
    // delta == 0
    constexpr uint64_t IDLE_TIMEOUT_MS = 99;
    if (t.lastDir != 0 && (now - t.lastMoveTickMs) < IDLE_TIMEOUT_MS) {
        return t.lastDir;
    }
    t.lastDir = 0;
    return 0;
}