#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>
#include <hidsdi.h>

struct ButtonMapping {
    USAGE usagePage = 0x09;
    USAGE usage = 0;
    ULONG linkCollection = 0;
    std::string name;
    size_t index = 0;
};

struct AxisMapping {
    USAGE usagePage = 0x01;
    USAGE usage = 0x30;
    ULONG linkCollection = 0;
    LONG logicalMin = 0;
    LONG logicalMax = 255;
    std::string name;
    size_t index = 0;
};

struct MyHidConfig {
    uint16_t vid = 0x034C;
    uint16_t pid = 0x0368;

    std::vector<ButtonMapping> buttons;
    std::vector<AxisMapping> axes;

    bool matches(uint16_t v, uint16_t p) const {
        return vid == v && pid == p;
    }
};

struct MyHidState {
    bool connected = false;

    std::vector<bool> dynamicButtons;
    std::vector<float> dynamicAxesNorm;
    std::vector<LONG> dynamicAxesRaw;
    std::vector<int> dynamicAxesDir;
};

struct HidOverlayState {
    bool connected = false;
    uint64_t tickMs = 0;

    std::vector<bool> buttons;
    std::vector<float> axesNorm;
    std::vector<LONG> axesRaw;
    std::vector<int> axesDir;
};

struct DeviceInfo {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint16_t vid = 0;
    uint16_t pid = 0;
    std::wstring name;
    USAGE usagePage = 0;
    USAGE usage = 0;
};