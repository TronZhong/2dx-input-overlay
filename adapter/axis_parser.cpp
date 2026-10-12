#include "axis_parser.h"

#include "platform/hid/hid_parser.h"
#include "adapter/my_hid_adapter.h"

#include <cstdint>
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

namespace axis_parser {

void parseAxes(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState,
    std::vector<MyHidAdapter::AxisTrack>& axisTracks) {

    if (config.axes.empty()) {
        outState.dynamicAxesNorm.clear();
        outState.dynamicAxesRaw.clear();
        outState.dynamicAxesDir.clear();
        return;
    }

    outState.dynamicAxesNorm.assign(config.axes.size(), 0.0f);
    outState.dynamicAxesRaw.assign(config.axes.size(), 0);
    outState.dynamicAxesDir.assign(config.axes.size(), 0);

    uint64_t now = currentTickMs();

    for (size_t i = 0; i < config.axes.size(); ++i) {
        const auto& axis = config.axes[i];
        LONG raw = 0;
        bool ok = hid_parser::getUsageValue(
            preparsed,
            axis.usagePage,
            static_cast<USHORT>(axis.linkCollection),
            axis.usage,
            rawData,
            static_cast<ULONG>(rawSize),
            raw);

        if (!ok) continue;

        outState.dynamicAxesRaw[i] = raw;

        // Normalize
        if (axis.logicalMax > axis.logicalMin) {
            float n = static_cast<float>(raw - axis.logicalMin) / static_cast<float>(axis.logicalMax - axis.logicalMin);
            if (n < 0.0f) n = 0.0f;
            if (n > 1.0f) n = 1.0f;
            outState.dynamicAxesNorm[i] = n;
        } else {
            outState.dynamicAxesNorm[i] = 0.0f;
        }

        // Direction tracking
        if (i >= axisTracks.size()) {
            axisTracks.resize(i + 1);
        }
        auto& t = axisTracks[i];
        LONG delta = 0;
        if (!t.hasPrev) {
            t.hasPrev = true;
        } else {
            delta = raw - t.prevRaw;
            if (delta > 128) delta -= 256;
            else if (delta < -128) delta += 256;
        }
        t.prevRaw = raw;

        int dir = 0;
        if (delta > 0) {
            t.lastDir = 1;
            t.lastMoveTickMs = now;
            dir = 1;
        } else if (delta < 0) {
            t.lastDir = -1;
            t.lastMoveTickMs = now;
            dir = -1;
        } else {
            constexpr uint64_t IDLE_TIMEOUT_MS = 99;
            if (t.lastDir != 0 && (now - t.lastMoveTickMs) < IDLE_TIMEOUT_MS) {
                dir = t.lastDir;
            } else {
                t.lastDir = 0;
                dir = 0;
            }
        }
        outState.dynamicAxesDir[i] = dir;
    }
}

} // namespace axis_parser