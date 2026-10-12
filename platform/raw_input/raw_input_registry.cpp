#include "raw_input_registry.h"

#include <windows.h>

bool RawInputRegistry::registerAll(HWND targetHwnd) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x00;
    rid.usUsage = 0x00;
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid.hwndTarget = targetHwnd;

    return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)) == TRUE;
}

bool RawInputRegistry::registerSpecific(HWND targetHwnd, USHORT usagePage, USHORT usage) {
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = usagePage;
    rid.usUsage = usage;
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid.hwndTarget = targetHwnd;

    return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)) == TRUE;
}