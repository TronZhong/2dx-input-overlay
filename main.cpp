#include <windows.h>
#include <hidsdi.h>

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "my_hid_adapter.h"

namespace {

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) {
        return {};
    }
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(sizeNeeded), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}

std::string getRawInputDeviceName(HANDLE deviceHandle) {
    UINT chars = 0;
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, nullptr, &chars) == static_cast<UINT>(-1) || chars == 0) {
        return {};
    }

    std::wstring name(chars, L'\0');
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, &name[0], &chars) == static_cast<UINT>(-1)) {
        return {};
    }

    if (!name.empty() && name.back() == L'\0') {
        name.pop_back();
    }
    return wideToUtf8(name);
}

struct TargetHidDevice {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::string name;
    std::vector<uint8_t> preparsed;
    MyHidState lastState {};
    bool hasState = false;
    uint64_t lastPrintMs = 0;
};

struct AppContext {
    MyHidAdapter adapter;
    MyHidConfig config;
    std::map<HANDLE, TargetHidDevice> devices;
};

struct OverlayState {
    bool button01 = false;
    bool button02 = false;
    bool button03 = false;
    bool button04 = false;
    bool button05 = false;
    bool button06 = false;
    bool button07 = false;
    float xNorm = 0.0f;
    int xDirection = 0;
    uint64_t tickMs = 0;
};

struct SpinLock {
    volatile LONG state = 0;

    void lock() {
        while (InterlockedCompareExchange(&state, 1, 0) != 0) {
            Sleep(0);
        }
    }

    void unlock() {
        InterlockedExchange(&state, 0);
    }
};

struct SpinLockGuard {
    explicit SpinLockGuard(SpinLock& s) : spin(s) {
        spin.lock();
    }

    ~SpinLockGuard() {
        spin.unlock();
    }

    SpinLock& spin;
};

struct SharedOverlayState {
    SpinLock lock;
    OverlayState latest {};
    bool hasData = false;
};

SharedOverlayState gOverlay;

void publishState(const MyHidState& s) {
    uint64_t now =
#if (_WIN32_WINNT >= 0x0600)
        GetTickCount64();
#else
        static_cast<uint64_t>(GetTickCount());
#endif

    OverlayState o {};
    o.button01 = s.button_01Pressed;
    o.button02 = s.button_02Pressed;
    o.button03 = s.button_03Pressed;
    o.button04 = s.button_04Pressed;
    o.button05 = s.button_05Pressed;
    o.button06 = s.button_06Pressed;
    o.button07 = s.button_07Pressed;
    o.xNorm = s.xNorm;
    o.xDirection = s.xDirection;
    o.tickMs = now;

    SpinLockGuard guard(gOverlay.lock);
    gOverlay.latest = o;
    gOverlay.hasData = true;
}

bool tryConsumeLatestState(OverlayState& out) {
    SpinLockGuard guard(gOverlay.lock);
    if (!gOverlay.hasData) {
        return false;
    }
    out = gOverlay.latest;
    return true;
}

void registerRawInput(HWND hwnd) {
    std::array<RAWINPUTDEVICE, 2> rid {};

    rid[0].usUsagePage = 0x01; // Generic Desktop
    rid[0].usUsage = 0x04;     // Joystick
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid[0].hwndTarget = hwnd;

    rid[1].usUsagePage = 0x01; // Generic Desktop
    rid[1].usUsage = 0x05;     // Game Pad
    rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid[1].hwndTarget = hwnd;

    if (!RegisterRawInputDevices(rid.data(), static_cast<UINT>(rid.size()), sizeof(RAWINPUTDEVICE))) {
        std::cerr << "RegisterRawInputDevices failed: " << GetLastError() << "\n";
    }
}

bool loadPreparsedData(HANDLE handle, std::vector<uint8_t>& out) {
    UINT size = 0;
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, nullptr, &size) == static_cast<UINT>(-1) || size == 0) {
        return false;
    }

    out.assign(size, 0);
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, out.data(), &size) == static_cast<UINT>(-1)) {
        return false;
    }

    return true;
}

bool tryAddTargetDevice(AppContext& app, HANDLE handle, bool printMatchedLog) {
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (app.devices.find(handle) != app.devices.end()) {
        return true;
    }

    RID_DEVICE_INFO info {};
    info.cbSize = sizeof(info);
    UINT infoSize = info.cbSize;
    if (GetRawInputDeviceInfo(handle, RIDI_DEVICEINFO, &info, &infoSize) == static_cast<UINT>(-1)) {
        return false;
    }

    if (info.dwType != RIM_TYPEHID) {
        return false;
    }

    uint16_t vid = static_cast<uint16_t>(info.hid.dwVendorId);
    uint16_t pid = static_cast<uint16_t>(info.hid.dwProductId);
    if (!app.adapter.matches(vid, pid)) {
        return false;
    }

    TargetHidDevice target {};
    target.handle = handle;
    target.name = getRawInputDeviceName(handle);
    if (!loadPreparsedData(handle, target.preparsed)) {
        std::cerr << "Failed to load preparsed data for matched HID.\n";
        return false;
    }

    app.devices[handle] = std::move(target);

    if (printMatchedLog) {
        std::cout << "Matched HID device: " << app.devices[handle].name
                  << " (VID=0x" << std::hex << vid << ", PID=0x" << pid << std::dec << ")\n";
        std::cout << "  [config] axisUsagePage=0x" << std::hex << app.config.axisUsagePage
                  << " xUsage=0x" << app.config.xUsage
                  << " axisLinkCollection=" << std::dec << app.config.axisLinkCollection << "\n";
    }

    return true;
}

void removeTargetDevice(AppContext& app, HANDLE handle) {
    auto it = app.devices.find(handle);
    if (it == app.devices.end()) {
        return;
    }

    std::cout << "Removed HID device: " << it->second.name << "\n";
    app.devices.erase(it);

    // Clear sticky overlay state when the last matched controller is unplugged.
    if (app.devices.empty()) {
        publishState(MyHidState {});
        std::cout << "No matched HID devices currently connected.\n";
    }
}


void scanTargetDevices(AppContext& app) {
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count == 0) {
        std::cerr << "No raw input devices found.\n";
        return;
    }

    std::vector<RAWINPUTDEVICELIST> list(count);
    if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
        std::cerr << "GetRawInputDeviceList failed.\n";
        return;
    }

    for (const auto& item : list) {
        if (item.dwType != RIM_TYPEHID) {
            continue;
        }

        tryAddTargetDevice(app, item.hDevice, true);
    }

    if (app.devices.empty()) {
        std::cout << "No HID matched target VID/PID.\n";
    }
}

void printStateIfChanged(TargetHidDevice& dev, const MyHidState& s) {
    constexpr float kAxisEpsilon = 0.002f;
    constexpr float kAxisImmediatePrintDelta = 0.03f;
    constexpr uint64_t kLogThrottleMs = 120;

    bool first = !dev.hasState;
    bool buttonChanged = first
        || dev.lastState.button_01Pressed != s.button_01Pressed
        || dev.lastState.button_02Pressed != s.button_02Pressed
        || dev.lastState.button_03Pressed != s.button_03Pressed
        || dev.lastState.button_04Pressed != s.button_04Pressed
        || dev.lastState.button_05Pressed != s.button_05Pressed
        || dev.lastState.button_06Pressed != s.button_06Pressed
        || dev.lastState.button_07Pressed != s.button_07Pressed
        || dev.lastState.xDirection != s.xDirection;

    float dx = first ? 1.0f : std::fabs(dev.lastState.xNorm - s.xNorm);
    bool axisChanged = dx > kAxisEpsilon;

    bool changed = buttonChanged || axisChanged;

    if (!changed) {
        return;
    }

    // Publish state updates immediately so render layer is not tied to log throttling.
    publishState(s);

        uint64_t now =
    #if (_WIN32_WINNT >= 0x0600)
        GetTickCount64();
    #else
        static_cast<uint64_t>(GetTickCount());
    #endif
    bool axisLargeMove = dx > kAxisImmediatePrintDelta;
    bool shouldThrottle = !buttonChanged && !axisLargeMove && (now - dev.lastPrintMs) < kLogThrottleMs;
    if (shouldThrottle) {
        dev.lastState = s;
        dev.hasState = true;
        return;
    }

    std::cout << "[HID] button_01=" << (s.button_01Pressed ? 1 : 0)
              << " button_02=" << (s.button_02Pressed ? 1 : 0)
              << " button_03=" << (s.button_03Pressed ? 1 : 0)
              << " button_04=" << (s.button_04Pressed ? 1 : 0)
              << " button_05=" << (s.button_05Pressed ? 1 : 0)
              << " button_06=" << (s.button_06Pressed ? 1 : 0)
              << " button_07=" << (s.button_07Pressed ? 1 : 0)

              << " x=" << s.xNorm
              << " xRaw=" << s.xRaw
              << " dir=" << s.xDirection
              << "\n";

    dev.lastPrintMs = now;
    dev.lastState = s;
    dev.hasState = true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<AppContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;
        }

        case WM_INPUT: {
            if (!app) {
                return 0;
            }

            UINT size = 0;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER))
                == static_cast<UINT>(-1) || size == 0) {
                return 0;
            }

            std::vector<uint8_t> buffer(size, 0);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER))
                != size) {
                return 0;
            }

            auto* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
            if (raw->header.dwType != RIM_TYPEHID) {
                return 0;
            }

            auto it = app->devices.find(raw->header.hDevice);
            if (it == app->devices.end()) {
                if (!tryAddTargetDevice(*app, raw->header.hDevice, true)) {
                    return 0;
                }
                it = app->devices.find(raw->header.hDevice);
                if (it == app->devices.end()) {
                    return 0;
                }
            }

            auto& dev = it->second;
            MyHidState state {};
            if (app->adapter.updateFromReport(
                    reinterpret_cast<PHIDP_PREPARSED_DATA>(dev.preparsed.data()),
                    raw->data.hid.bRawData,
                    raw->data.hid.dwSizeHid,
                    state)) {
                printStateIfChanged(dev, state);
            }
            return 0;
        }

        case WM_INPUT_DEVICE_CHANGE: {
            if (!app) {
                return 0;
            }

            HANDLE handle = reinterpret_cast<HANDLE>(lParam);
            if (wParam == GIDC_ARRIVAL) {
                tryAddTargetDevice(*app, handle, true);
            } else if (wParam == GIDC_REMOVAL) {
                removeTargetDevice(*app, handle);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int main() {
    AppContext app { MyHidAdapter(MyHidConfig {}), MyHidConfig {}, {} };

    // Fill with your real controller values.
    app.config.vid = 0x034C;
    app.config.pid = 0x0368;
    app.adapter = MyHidAdapter(app.config);

    WNDCLASSW wc {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SingleHidMonitorWnd";

    if (!RegisterClassW(&wc)) {
        std::cerr << "RegisterClass failed: " << GetLastError() << "\n";
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Single HID Monitor",
        0,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        wc.hInstance,
        &app);

    if (!hwnd) {
        std::cerr << "CreateWindowEx failed: " << GetLastError() << "\n";
        return 1;
    }

    registerRawInput(hwnd);
    scanTargetDevices(app);

    std::cout << "Listening for WM_INPUT... Press Ctrl+C to stop.\n";

    MSG msg {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
