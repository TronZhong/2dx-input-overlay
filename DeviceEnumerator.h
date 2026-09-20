#pragma once

#include <windows.h>
#include <hidsdi.h>
#include <cstdint>
#include <string>
#include <vector>

struct DeviceInfo {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint16_t vid = 0;
    uint16_t pid = 0;
    std::wstring name;
    USAGE usagePage = 0;
    USAGE usage = 0;
};

class DeviceEnumerator {
public:
    static std::vector<DeviceInfo> enumerateAll();
};