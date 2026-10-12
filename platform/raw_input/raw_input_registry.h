#pragma once

#include <windows.h>

class RawInputRegistry {
public:
    // Register for all HID devices (wildcard usage page/usage) with input sink + device notify
    static bool registerAll(HWND targetHwnd);
    
    // Register for specific usage page/usage (legacy, not used in new architecture)
    static bool registerSpecific(HWND targetHwnd, USHORT usagePage, USHORT usage);
};