#pragma once

#include <windows.h>
#include <hidsdi.h>
#include <vector>
#include <string>

struct RawInputData {
    HANDLE deviceHandle = nullptr;
    std::vector<uint8_t> rawData;
    UINT dataSize = 0;
};

class RawInputParser {
public:
    // Parse WM_INPUT lParam into RawInputData
    static bool parseRawInput(LPARAM lParam, RawInputData& out);

    // Get preparsed data for a device handle
    static bool getPreparsedData(HANDLE handle, std::vector<uint8_t>& out);

    // Get device name for a handle
    static std::string getDeviceName(HANDLE handle);

    // Get device info (VID/PID/Usage) for a handle
    static bool getDeviceInfo(HANDLE handle, RID_DEVICE_INFO& out);
};