#pragma once

#include <windows.h>

#include <hidsdi.h>

#include <cstdint>
#include <string>
#include <vector>

// Describes one automatically-detected button from a device's HID descriptor.
struct DetectedButton {
    USAGE usage = 0;            // Button usage on the button page
    ULONG linkCollection = 0;   // Link collection that owns the button
    std::string name;           // Human-friendly label
};

// Describes one automatically-detected value (axis / scalar) usage.
struct DetectedValue {
    USAGE usagePage = 0;   // Usage page (e.g. 0x01 generic desktop)
    USAGE usage = 0;       // Usage ID (e.g. 0x30 X)
    ULONG linkCollection = 0;
    LONG logicalMin = 0;
    LONG logicalMax = 0;
    bool isRange = false;  // true if this value is a range (has logical min/max)
    std::string name;      // Human-friendly label
};

// Result of probing one device's capabilities.
struct HidCapabilities {
    bool valid = false;
    std::vector<DetectedButton> buttons;
    std::vector<DetectedValue> values;
};

class CapabilityInspector {
public:
    // Enumerate all button usages and value usages from a preparsed HID report.
    static HidCapabilities enumerate(PHIDP_PREPARSED_DATA preparsed);
};
