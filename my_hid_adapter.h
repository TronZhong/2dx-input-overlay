#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>

extern "C" {
#include <hidsdi.h>
}

struct MyHidConfig {
    // Match one device by VID/PID
    uint16_t vid = 0x034C;
    uint16_t pid = 0x0368;

    // Buttons
    USAGE buttonUsagePage = 0x09; // Button page
    USAGE button_01Usage = 0x01;
    USAGE button_02Usage = 0x02;
    USAGE button_03Usage = 0x03;
    USAGE button_04Usage = 0x04;
    USAGE button_05Usage = 0x05;
    USAGE button_06Usage = 0x06;
    USAGE button_07Usage = 0x07;
    ULONG buttonLinkCollection = 0;

    // Axes
    USAGE axisUsagePage = 0x01; // Generic Desktop page
    USAGE xUsage = 0x30;        // X
    ULONG axisLinkCollection = 0;

    // Rotary controllers often only expose one meaningful axis.
    bool enableYAxis = false;

    LONG xLogicalMin = 0;
    LONG xLogicalMax = 255;

    // Keep the last non-zero direction briefly while reports temporarily flatten to 0.
    uint32_t xIdleTimeoutMs = 100;
};

struct MyHidState {
    bool connected = false;
    bool button_01Pressed = false;
    bool button_02Pressed = false;
    bool button_03Pressed = false;
    bool button_04Pressed = false;
    bool button_05Pressed = false;
    bool button_06Pressed = false;
    bool button_07Pressed = false;

    float xNorm = 0.0f; // normalized to [0, 1]

    // raw X from HID report, useful for tuning
    LONG xRaw = 0;
    LONG xDeltaRaw = 0; // change since last report

    // -1: counter-clockwise, 0: idle/unknown, 1: clockwise
    int xDirection = 0;
};

class MyHidAdapter {
public:
    explicit MyHidAdapter(MyHidConfig cfg = {}) : cfg_(cfg) {}

    bool matches(uint16_t vid, uint16_t pid) const;

    // Parse one HID input report.
    bool updateFromReport(
        PHIDP_PREPARSED_DATA preparsed,
        const uint8_t* rawData,
        size_t rawSize,
        MyHidState& outState) const;

private:
    MyHidConfig cfg_;

    mutable bool hasPrevXRaw = false;
    mutable LONG prevXRaw = 0;
    mutable int lastXDirection = 0;
    mutable uint64_t lastXMoveTickMs = 0;

    static float normalizeToUnit(LONG value, LONG logicalMin, LONG logicalMax);
};
