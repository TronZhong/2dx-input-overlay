#include "my_hid_adapter.h"

#include <algorithm>
#include <iostream>

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
    return vid == cfg_.vid && pid == cfg_.pid;
}

bool MyHidAdapter::updateFromReport(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    MyHidState& outState) const {

    if (!preparsed || !rawData || rawSize == 0) {
        return false;
    }

    outState.connected = true;

    // Helper: read a single usage value
    auto readUsageValue = [&](USAGE usagePage, USAGE linkCollection, USAGE usage, LONG& outRaw) -> bool {
        LONG raw = 0;
        auto status = HidP_GetUsageValue(
            HidP_Input,
            usagePage,
            linkCollection,
            usage,
            reinterpret_cast<PULONG>(&raw),
            preparsed,
            reinterpret_cast<PCHAR>(const_cast<uint8_t*>(rawData)),
            static_cast<ULONG>(rawSize));

        if (status == HIDP_STATUS_SUCCESS) {
            outRaw = raw;
            return true;
        }
        return false;
    };

    // Helper: get all active button usages from report
    auto getActiveButtonUsages = [&](USAGE usagePage, ULONG linkCollection, std::vector<USAGE>& outUsages) -> bool {
        ULONG usageLength = 64;
        outUsages.assign(usageLength, 0);
        auto btnStatus = HidP_GetUsages(
            HidP_Input,
            usagePage,
            static_cast<USAGE>(linkCollection),
            outUsages.data(),
            &usageLength,
            preparsed,
            reinterpret_cast<PCHAR>(const_cast<uint8_t*>(rawData)),
            static_cast<ULONG>(rawSize));

        if (btnStatus == HIDP_STATUS_SUCCESS) {
            outUsages.resize(usageLength);
            return true;
        }
        return false;
    };

    // ===== BUTTONS =====
    std::vector<USAGE> activeUsages;
    bool useDynamicButtons = !cfg_.buttons.empty();

    if (useDynamicButtons) {
        // Use first button mapping's usagePage/linkCollection for the usage list query
        const auto& firstBtn = cfg_.buttons[0];
        getActiveButtonUsages(firstBtn.usagePage, firstBtn.linkCollection, activeUsages);

        // Initialize all legacy button fields to false
        outState.button_01Pressed = false;
        outState.button_02Pressed = false;
        outState.button_03Pressed = false;
        outState.button_04Pressed = false;
        outState.button_05Pressed = false;
        outState.button_06Pressed = false;
        outState.button_07Pressed = false;

        // Map dynamic buttons to legacy fields (by index, up to 7)
        for (size_t i = 0; i < cfg_.buttons.size() && i < 7; ++i) {
            const auto& btn = cfg_.buttons[i];
            bool pressed = false;
            for (USAGE u : activeUsages) {
                if (u == btn.usage) {
                    pressed = true;
                    break;
                }
            }
            // Write to legacy field by index
            switch (i) {
                case 0: outState.button_01Pressed = pressed; break;
                case 1: outState.button_02Pressed = pressed; break;
                case 2: outState.button_03Pressed = pressed; break;
                case 3: outState.button_04Pressed = pressed; break;
                case 4: outState.button_05Pressed = pressed; break;
                case 5: outState.button_06Pressed = pressed; break;
                case 6: outState.button_07Pressed = pressed; break;
            }
        }
    } else {
        // Legacy path
        getActiveButtonUsages(cfg_.buttonUsagePage, cfg_.buttonLinkCollection, activeUsages);

        bool button_01 = false, button_02 = false, button_03 = false;
        bool button_04 = false, button_05 = false, button_06 = false, button_07 = false;

        for (ULONG i = 0; i < activeUsages.size(); ++i) {
            if (activeUsages[i] == cfg_.button_01Usage) button_01 = true;
            if (activeUsages[i] == cfg_.button_02Usage) button_02 = true;
            if (activeUsages[i] == cfg_.button_03Usage) button_03 = true;
            if (activeUsages[i] == cfg_.button_04Usage) button_04 = true;
            if (activeUsages[i] == cfg_.button_05Usage) button_05 = true;
            if (activeUsages[i] == cfg_.button_06Usage) button_06 = true;
            if (activeUsages[i] == cfg_.button_07Usage) button_07 = true;
        }

        outState.button_01Pressed = button_01;
        outState.button_02Pressed = button_02;
        outState.button_03Pressed = button_03;
        outState.button_04Pressed = button_04;
        outState.button_05Pressed = button_05;
        outState.button_06Pressed = button_06;
        outState.button_07Pressed = button_07;
    }

    // ===== AXES =====
    bool useDynamicAxes = !cfg_.axes.empty();
    LONG rawX = 0;
    bool gotX = false;

    if (useDynamicAxes) {
        // Use first axis mapping
        const auto& axis = cfg_.axes[0];
        gotX = readUsageValue(axis.usagePage, axis.linkCollection, axis.usage, rawX);
        // Store logical range for normalization
        if (gotX) {
            outState.xRaw = rawX;
            outState.xNorm = normalizeToUnit(rawX, axis.logicalMin, axis.logicalMax);
        }
    } else {
        // Legacy path with fallback
        gotX = readUsageValue(cfg_.axisUsagePage, static_cast<USAGE>(cfg_.axisLinkCollection), cfg_.xUsage, rawX);
        if (!gotX) {
            // Fallback probing (same as before)
            const USAGE candidates[] = {
                0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39
            };
            for (USAGE link = 0; link <= 4 && !gotX; ++link) {
                for (auto usage : candidates) {
                    if (readUsageValue(cfg_.axisUsagePage, link, usage, rawX)) {
                        gotX = true;
                        static bool printedXFallback = false;
                        if (!printedXFallback) {
                            std::cout << "[axis-fallback] X mapped to usage=0x" << std::hex << usage
                                      << " link=" << std::dec << link << "\n";
                            printedXFallback = true;
                        }
                        break;
                    }
                }
            }
        }
        if (gotX) {
            outState.xRaw = rawX;
            outState.xNorm = normalizeToUnit(rawX, cfg_.xLogicalMin, cfg_.xLogicalMax);
        }
    }

    // ===== DIRECTION CALCULATION (shared) =====
    if (gotX) {
        uint64_t now = currentTickMs();

        if (!hasPrevXRaw) {
            outState.xDeltaRaw = 0;
            hasPrevXRaw = true;
        } else {
            LONG delta = rawX - prevXRaw;
            if (delta > 128) delta -= 256;
            else if (delta < -128) delta += 256;
            outState.xDeltaRaw = delta;
        }

        prevXRaw = rawX;

        if (outState.xDeltaRaw > 0) {
            lastXDirection = 1;
            lastXMoveTickMs = now;
            outState.xDirection = 1;
        } else if (outState.xDeltaRaw < 0) {
            lastXDirection = -1;
            lastXMoveTickMs = now;
            outState.xDirection = -1;
        } else {
            uint64_t idleMs = now - lastXMoveTickMs;
            if (lastXDirection != 0 && idleMs < cfg_.xIdleTimeoutMs) {
                outState.xDirection = lastXDirection;
            } else {
                lastXDirection = 0;
                outState.xDirection = 0;
            }
        }

        // Debug samples
        static int rawSampleCount = 0;
        if (rawSampleCount < 2) {
            std::cout << "[axis-raw] x=" << rawX
                      << " delta=" << outState.xDeltaRaw
                      << " dir=" << outState.xDirection << "\n";
            rawSampleCount++;
        }
    } else {
        outState.xRaw = 0;
        outState.xNorm = 0.0f;
        outState.xDeltaRaw = 0;
        outState.xDirection = 0;
        lastXDirection = 0;
        lastXMoveTickMs = 0;
        hasPrevXRaw = false;
        prevXRaw = 0;
    }

    return true;
}

bool MyHidAdapter::autoDetect(const HidCapabilities& caps) {
    if (!caps.valid) {
        return false;
    }

    // Build button mappings from detected button caps.
    cfg_.buttons.clear();
    for (size_t i = 0; i < caps.buttons.size(); ++i) {
        const auto& b = caps.buttons[i];
        ButtonMapping m;
        m.usagePage = 0x09;   // Button page
        m.usage = b.usage;
        m.linkCollection = b.linkCollection;
        m.name = b.name;
        m.index = i;
        cfg_.buttons.push_back(m);
    }

    // Build axis mappings from detected range values.
    cfg_.axes.clear();
    for (size_t i = 0; i < caps.values.size(); ++i) {
        const auto& v = caps.values[i];
        // Only treat range-valued usages as axes (buttons are separate page).
        if (!v.isRange) {
            continue;
        }
        AxisMapping m;
        m.usagePage = v.usagePage;
        m.usage = v.usage;
        m.linkCollection = v.linkCollection;
        m.logicalMin = v.logicalMin;
        m.logicalMax = v.logicalMax;
        m.name = v.name;
        m.index = i;
        cfg_.axes.push_back(m);
    }

    return !cfg_.buttons.empty() || !cfg_.axes.empty();
}

float MyHidAdapter::normalizeToUnit(LONG value, LONG logicalMin, LONG logicalMax) {
    if (logicalMax <= logicalMin) {
        return 0.0f;
    }

    float n = static_cast<float>(value - logicalMin) /
              static_cast<float>(logicalMax - logicalMin);

    return std::clamp(n, 0.0f, 1.0f);
}