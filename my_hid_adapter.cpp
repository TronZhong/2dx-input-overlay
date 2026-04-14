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

}

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

    // 1) Parse button usages currently active.
    ULONG usageLength = 64;
    std::vector<USAGE> usages(usageLength);
    auto btnStatus = HidP_GetUsages(
        HidP_Input,
        cfg_.buttonUsagePage,
        cfg_.buttonLinkCollection,
        usages.data(),
        &usageLength,
        preparsed,
        reinterpret_cast<PCHAR>(const_cast<uint8_t*>(rawData)),
        static_cast<ULONG>(rawSize));

    if (btnStatus == HIDP_STATUS_SUCCESS) {
        bool button_01 = false;
        bool button_02 = false;
        bool button_03 = false;
        bool button_04 = false;
        bool button_05 = false;
        bool button_06 = false;
        bool button_07 = false;

        for (ULONG i = 0; i < usageLength; i++) {
            if (usages[i] == cfg_.button_01Usage) {
                button_01 = true;
            }
            if (usages[i] == cfg_.button_02Usage) {
                button_02 = true;
            }
            if (usages[i] == cfg_.button_03Usage) {
                button_03 = true;
            }
            if (usages[i] == cfg_.button_04Usage) {
                button_04 = true;
            }
            if (usages[i] == cfg_.button_05Usage) {
                button_05 = true;
            }
            if (usages[i] == cfg_.button_06Usage) {
                button_06 = true;
            }
            if (usages[i] == cfg_.button_07Usage) {
                button_07 = true;
            }
        }

        outState.button_01Pressed = button_01;
        outState.button_02Pressed = button_02;
        outState.button_03Pressed = button_03;
        outState.button_04Pressed = button_04;
        outState.button_05Pressed = button_05;
        outState.button_06Pressed = button_06;
        outState.button_07Pressed = button_07;
    }

    auto readUsageValue = [&](USAGE usagePage, ULONG linkCollection, USAGE usage, LONG& outRaw) -> bool {
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

    auto tryCommonAxisFallback = [&](LONG& outRaw, USAGE& usedUsage, ULONG& usedLink) -> bool {
        const USAGE candidates[] = {
            0x30, // X
            0x31, // Y
            0x32, // Z
            0x33, // Rx
            0x34, // Ry
            0x35, // Rz
            0x36, // Slider
            0x37, // Dial
            0x38, // Wheel
            0x39  // Hat switch
        };

        for (ULONG link = 0; link <= 4; link++) {
            for (auto usage : candidates) {
                if (readUsageValue(cfg_.axisUsagePage, link, usage, outRaw)) {
                    usedUsage = usage;
                    usedLink = link;
                    return true;
                }
            }
        }
        return false;
    };

    // 2) Parse X axis.
    LONG rawX = 0;
    bool gotX = readUsageValue(cfg_.axisUsagePage, cfg_.axisLinkCollection, cfg_.xUsage, rawX);
    if (!gotX) {
        USAGE foundUsage = 0;
        ULONG foundLink = 0;
        gotX = tryCommonAxisFallback(rawX, foundUsage, foundLink);
        static bool printedXFallback = false;
        if (gotX && !printedXFallback) {
            std::cout << "[axis-fallback] X mapped to usage=0x" << std::hex << foundUsage
                      << " link=" << std::dec << foundLink << "\n";
            printedXFallback = true;
        }
    }

    if (gotX) {
        uint64_t now = currentTickMs();
        outState.xRaw = rawX;
        outState.xNorm = normalizeToUnit(rawX, cfg_.xLogicalMin, cfg_.xLogicalMax);

        if (!hasPrevXRaw) {
            outState.xDeltaRaw = 0;
            hasPrevXRaw = true;
        } else {
            LONG delta = rawX - prevXRaw;
            if (delta > 128) {
                delta -= 256;
            } else if (delta < -128) {
                delta += 256;
            }
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
    } else {
        outState.xRaw = 0;
        outState.xNorm = 0.0f;
        outState.xDeltaRaw = 0;
        outState.xDirection = 0;
        lastXDirection = 0;
        lastXMoveTickMs = 0;
    }

    // Print a few raw samples to help tune logical min/max.
    static int rawSampleCount = 0;
    if (rawSampleCount < 2 && gotX) {
        std::cout << "[axis-raw] x=" << rawX
                  << " delta=" << outState.xDeltaRaw
                  << " dir=" << outState.xDirection << "\n";
        rawSampleCount++;
    }

    return true;
}

float MyHidAdapter::normalizeToUnit(LONG value, LONG logicalMin, LONG logicalMax) {
    if (logicalMax <= logicalMin) {
        return 0.0f;
    }

    float n = static_cast<float>(value - logicalMin) /
              static_cast<float>(logicalMax - logicalMin);

    return std::clamp(n, 0.0f, 1.0f);
}
