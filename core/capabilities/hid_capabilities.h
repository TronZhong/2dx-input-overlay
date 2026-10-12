#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>
#include <hidsdi.h>

struct DetectedButton {
    USAGE usage = 0;
    ULONG linkCollection = 0;
    std::string name;
};

struct DetectedValue {
    USAGE usagePage = 0;
    USAGE usage = 0;
    ULONG linkCollection = 0;
    LONG logicalMin = 0;
    LONG logicalMax = 0;
    bool isRange = false;
    std::string name;
};

struct HidCapabilities {
    bool valid = false;
    std::vector<DetectedButton> buttons;
    std::vector<DetectedValue> values;
};

class CapabilityInspector {
public:
    static HidCapabilities enumerate(PHIDP_PREPARSED_DATA preparsed);
};