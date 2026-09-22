#include "hid_input_backend.h"

#include <windows.h>

#include <hidsdi.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

uint64_t currentTickMs() {
#if (_WIN32_WINNT >= 0x0600)
    return GetTickCount64();
#else
    return static_cast<uint64_t>(GetTickCount());
#endif
}

} // namespace

// Impl keeps platform details private so the public header stays small and
// easy to embed into another project (for example an OBS source plugin).
struct HidInputBackend::Impl {
    struct TargetHidDevice {
        HANDLE handle = INVALID_HANDLE_VALUE;
        std::string name;
        std::vector<uint8_t> preparsed;

        // Which button/axis usages this top-level collection owns. Because a
        // composite device (e.g. a mouse) exposes several collections in Raw Input,
        // each report can only be decoded against its own collection's preparsed
        // data. These sets let us merge per-collection states without clobbering
        // signals that belong to another collection.
        std::vector<USAGE> buttonUsages;                          // button usages
        std::vector<std::pair<USHORT, USAGE>> axisUsages;          // (usagePage, usage)
    };

    explicit Impl(MyHidConfig cfg)
        : adapter(cfg), config(cfg) {
    }

    ~Impl() {
        stop();
    }

    MyHidAdapter adapter;
    MyHidConfig config;

    // Aggregated state across all top-level collections of the target device.
    // Each collection updates only the button/axis slices it owns.
    MyHidState master;

    std::map<HANDLE, TargetHidDevice> devices;

    // Worker owns the hidden window and receives WM_INPUT/WM_INPUT_DEVICE_CHANGE.
    std::atomic<bool> running { false };
    std::thread worker;
    DWORD workerThreadId = 0;
    HANDLE readyEvent = nullptr;
    HWND hwnd = nullptr;

    // Single producer (input thread) + single consumer (render/main thread)
    // currently guarded by mutex for clarity and correctness.
    mutable std::mutex stateMutex;
    HidOverlayState latest {};
    bool hasLatest = false;

    static constexpr const wchar_t* kClassName = L"HidInputBackendWnd";

    bool start() {
        if (running.load()) {
            return true;
        }

        readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!readyEvent) {
            return false;
        }

        running.store(true);
        worker = std::thread([this]() { threadMain(); });

        DWORD waitRes = WaitForSingleObject(readyEvent, 5000);
        if (waitRes != WAIT_OBJECT_0 || hwnd == nullptr) {
            stop();
            return false;
        }

        return true;
    }

    void stop() {
        if (!running.exchange(false)) {
            if (readyEvent) {
                CloseHandle(readyEvent);
                readyEvent = nullptr;
            }
            return;
        }

        if (workerThreadId != 0) {
            PostThreadMessage(workerThreadId, WM_QUIT, 0, 0);
        }

        if (worker.joinable()) {
            worker.join();
        }

        devices.clear();
        workerThreadId = 0;
        hwnd = nullptr;

        if (readyEvent) {
            CloseHandle(readyEvent);
            readyEvent = nullptr;
        }

        std::lock_guard<std::mutex> guard(stateMutex);
        latest = HidOverlayState {};
        hasLatest = false;
    }

    bool tryGetLatest(HidOverlayState& out) const {
        std::lock_guard<std::mutex> guard(stateMutex);
        if (!hasLatest) {
            return false;
        }
        out = latest;
        return true;
    }

    static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        if (msg == WM_CREATE) {
            auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return 0;
        }

        if (!self) {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        return self->wndProc(hwnd, msg, wParam, lParam);
    }

    LRESULT wndProc(HWND, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_INPUT:
                // Input report arrived for one HID device.
                onRawInput(lParam);
                return 0;

            case WM_INPUT_DEVICE_CHANGE: {
                // Hot-plug notifications from Raw Input registration.
                HANDLE handle = reinterpret_cast<HANDLE>(lParam);
                if (wParam == GIDC_ARRIVAL) {
                    tryAddTargetDevice(handle, true);
                } else if (wParam == GIDC_REMOVAL) {
                    removeTargetDevice(handle);
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

    void threadMain() {
        workerThreadId = GetCurrentThreadId();

        WNDCLASSW wc {};
        wc.lpfnWndProc = staticWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;

        RegisterClassW(&wc);

        hwnd = CreateWindowExW(
            0,
            kClassName,
            L"HID Input Backend",
            0,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr,
            wc.hInstance,
            this);

        if (hwnd) {
            // Bind to joystick/gamepad raw input and perform initial scan.
            registerRawInput(hwnd);
            scanTargetDevices();
        }

        if (readyEvent) {
            SetEvent(readyEvent);
        }

        MSG msg {};
        // Standard Win32 message loop; all HID callbacks happen on this thread.
        while (running.load() && GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }

        UnregisterClassW(kClassName, wc.hInstance);
    }

    void registerRawInput(HWND targetHwnd) {
        // Register wildcard page/usage so ALL HID devices generate WM_INPUT.
        // Composite devices (mouse, etc.) have top-level collections whose usage is
        // not joystick/gamepad (0x04/0x05); without this the mouse would never be
        // delivered. Whether a device is actually processed is decided later in
        // tryAddTargetDevice by the configured VID/PID filter.
        RAWINPUTDEVICE rid {};
        rid.usUsagePage = 0x00;
        rid.usUsage = 0x00;
        rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        rid.hwndTarget = targetHwnd;

        RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
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

    // Discover which button/axis usages this collection owns by enumerating its caps.
    void indexDeviceCapabilities(TargetHidDevice& dev) {
        dev.buttonUsages.clear();
        dev.axisUsages.clear();
        if (dev.preparsed.empty()) {
            return;
        }
        HidCapabilities caps = CapabilityInspector::enumerate(
            reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(dev.preparsed.data())));
        if (!caps.valid) {
            return;
        }
        for (const auto& b : caps.buttons) {
            dev.buttonUsages.push_back(b.usage);
        }
        for (const auto& v : caps.values) {
            if (v.isRange) {
                dev.axisUsages.emplace_back(static_cast<USHORT>(v.usagePage), v.usage);
            }
        }
    }

    bool ownsButton(const TargetHidDevice& dev, USAGE usage) const {
        for (USAGE u : dev.buttonUsages) {
            if (u == usage) return true;
        }
        return false;
    }

    bool ownsAxis(const TargetHidDevice& dev, USAGE usagePage, USAGE usage) const {
        for (const auto& p : dev.axisUsages) {
            if (p.first == static_cast<USHORT>(usagePage) && p.second == usage) return true;
        }
        return false;
    }

    bool tryAddTargetDevice(HANDLE handle, bool printMatchedLog) {
        if (!handle || handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        if (devices.find(handle) != devices.end()) {
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
        // Filter devices by configured VID/PID before expensive parsing work.
        if (!adapter.matches(vid, pid)) {
            return false;
        }

        TargetHidDevice target {};
        target.handle = handle;
        target.name = getRawInputDeviceName(handle);
        if (!loadPreparsedData(handle, target.preparsed)) {
            return false;
        }

        // Index which button/axis usages this collection owns so per-collection
        // states can be merged without clobbering other collections' signals.
        indexDeviceCapabilities(target);

        devices[handle] = std::move(target);

        if (printMatchedLog) {
            const auto& added = devices[handle];
            OutputDebugStringA(("[hid] Matched device: " + added.name + "\n").c_str());
        }

        return true;
    }

    void removeTargetDevice(HANDLE handle) {
        auto it = devices.find(handle);
        if (it == devices.end()) {
            return;
        }

        devices.erase(it);

        if (devices.empty()) {
            // Follow selected UX: unplugging the last target clears overlay state.
            publishDisconnectedState();
        }
    }

    void scanTargetDevices() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count == 0) {
            return;
        }

        std::vector<RAWINPUTDEVICELIST> list(count);
        if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
            return;
        }

        for (const auto& item : list) {
            if (item.dwType != RIM_TYPEHID) {
                continue;
            }
            tryAddTargetDevice(item.hDevice, true);
        }

        if (devices.empty()) {
            publishDisconnectedState();
        }
    }

    void resetMasterState() {
        master = MyHidState {};
    }

    void setTargetVidPid(uint16_t vid, uint16_t pid) {
        config.vid = vid;
        config.pid = pid;
        // Recreate adapter with new config (adapter is const in updateFromReport but holds config by value)
        adapter = MyHidAdapter(config);
        resetMasterState();
        // Clear current devices and re-scan
        devices.clear();
        scanTargetDevices();
    }

    bool autoConfigure(uint16_t vid, uint16_t pid, HidCapabilities* outCaps) {
        config.vid = vid;
        config.pid = pid;
        adapter = MyHidAdapter(config);
        resetMasterState();

        // Clear devices and scan so all matching collections are bound and their
        // preparsed data loaded.
        devices.clear();
        scanTargetDevices();

        // Merge capabilities across every top-level collection of the target
        // device. A composite device (e.g. a mouse) exposes several collections in
        // Raw Input, each a separate handle; a single collection often reports no
        // button/value caps, so only aggregating them yields the full mapping.
        HidCapabilities merged;
        bool any = false;

        for (const auto& kv : devices) {
            const TargetHidDevice& dev = kv.second;
            if (dev.preparsed.empty()) {
                continue;
            }
            HidCapabilities caps =
                CapabilityInspector::enumerate(reinterpret_cast<PHIDP_PREPARSED_DATA>(
                    const_cast<uint8_t*>(dev.preparsed.data())));
            if (!caps.valid) {
                continue;
            }
            any = true;

            // Append buttons, deduplicated by (usage, linkCollection).
            for (auto& b : caps.buttons) {
                bool dup = false;
                for (const auto& m : merged.buttons) {
                    if (m.usage == b.usage && m.linkCollection == b.linkCollection) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    merged.buttons.push_back(std::move(b));
                }
            }

            // Append values/axes, deduplicated by (usagePage, usage, linkCollection).
            for (auto& v : caps.values) {
                bool dup = false;
                for (const auto& m : merged.values) {
                    if (m.usagePage == v.usagePage && m.usage == v.usage
                        && m.linkCollection == v.linkCollection) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    merged.values.push_back(std::move(v));
                }
            }
        }

        if (!any) {
            return false;
        }
        merged.valid = true;

        // Apply detected mappings to the adapter, then sync the config.
        if (adapter.autoDetect(merged)) {
            config = adapter.config();
        }
        if (outCaps) {
            *outCaps = std::move(merged);
        }
        return true;
    }

    void onRawInput(LPARAM lParam) {
        UINT size = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER))
            == static_cast<UINT>(-1) || size == 0) {
            return;
        }

        std::vector<uint8_t> buffer(size, 0);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
            return;
        }

        auto* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
        if (raw->header.dwType != RIM_TYPEHID) {
            return;
        }

        auto it = devices.find(raw->header.hDevice);
        if (it == devices.end()) {
            // Lazy bind helps on edge cases where input arrives before change event.
            if (!tryAddTargetDevice(raw->header.hDevice, true)) {
                return;
            }
            it = devices.find(raw->header.hDevice);
            if (it == devices.end()) {
                return;
            }
        }

        const TargetHidDevice& dev = it->second;

        // Decode this report against its own collection's preparsed data. The
        // adapter fills all dynamic arrays, but only the usages owned by this
        // collection are meaningful; merge just that slice into the master state
        // so one collection's report doesn't clobber another's signals.
        MyHidState slice {};
        if (!adapter.updateFromReport(
                reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(dev.preparsed.data())),
                raw->data.hid.bRawData,
                raw->data.hid.dwSizeHid,
                slice)) {
            return;
        }

        // Ensure master arrays are sized to the current config. Only (re)allocate
        // when the mapping size changes (e.g. after selecting another device); do
        // NOT reset every report, otherwise one collection would wipe the slices
        // owned by other collections.
        const auto& btns = config.buttons;
        const auto& axes = config.axes;
        if (master.dynamicButtons.size() != btns.size()) {
            master.dynamicButtons.assign(btns.size(), false);
        }
        if (master.dynamicAxesNorm.size() != axes.size()) {
            master.dynamicAxesNorm.assign(axes.size(), 0.0f);
            master.dynamicAxesRaw.assign(axes.size(), 0);
            master.dynamicAxesDir.assign(axes.size(), 0);
        }

        for (size_t i = 0; i < btns.size() && i < slice.dynamicButtons.size(); ++i) {
            if (ownsButton(dev, btns[i].usage)) {
                master.dynamicButtons[i] = slice.dynamicButtons[i];
            }
        }
        for (size_t i = 0; i < axes.size() && i < slice.dynamicAxesRaw.size(); ++i) {
            if (ownsAxis(dev, axes[i].usagePage, axes[i].usage)) {
                master.dynamicAxesNorm[i] = slice.dynamicAxesNorm[i];
                master.dynamicAxesRaw[i] = slice.dynamicAxesRaw[i];
                master.dynamicAxesDir[i] = slice.dynamicAxesDir[i];
            }
        }

        // First-owning axis / any button feed the legacy fields.
        if (!slice.dynamicAxesRaw.empty() && ownsAxis(dev, axes.empty() ? 0x01 : axes[0].usagePage,
                                                       axes.empty() ? 0x30 : axes[0].usage)) {
            master.xNorm = slice.xNorm;
            master.xDirection = slice.xDirection;
        }
        for (size_t i = 0; i < btns.size() && i < 7; ++i) {
            if (ownsButton(dev, btns[i].usage)) {
                switch (i) {
                    case 0: master.button_01Pressed = slice.dynamicButtons[i]; break;
                    case 1: master.button_02Pressed = slice.dynamicButtons[i]; break;
                    case 2: master.button_03Pressed = slice.dynamicButtons[i]; break;
                    case 3: master.button_04Pressed = slice.dynamicButtons[i]; break;
                    case 4: master.button_05Pressed = slice.dynamicButtons[i]; break;
                    case 5: master.button_06Pressed = slice.dynamicButtons[i]; break;
                    case 6: master.button_07Pressed = slice.dynamicButtons[i]; break;
                }
            }
        }
        master.connected = true;

        publishState(master);
    }

    void publishState(const MyHidState& state) {
        HidOverlayState next {};
        next.connected = true;
        next.button01 = state.button_01Pressed;
        next.button02 = state.button_02Pressed;
        next.button03 = state.button_03Pressed;
        next.button04 = state.button_04Pressed;
        next.button05 = state.button_05Pressed;
        next.button06 = state.button_06Pressed;
        next.button07 = state.button_07Pressed;
        next.xNorm = state.xNorm;
        next.xDirection = state.xDirection;
        next.buttons = state.dynamicButtons;
        next.axesNorm = state.dynamicAxesNorm;
        next.axesRaw = state.dynamicAxesRaw;
        next.axesDir = state.dynamicAxesDir;
        next.tickMs = currentTickMs();

        std::lock_guard<std::mutex> guard(stateMutex);
        latest = next;
        hasLatest = true;
    }

    void publishDisconnectedState() {
        HidOverlayState next {};
        next.connected = false;
        next.tickMs = currentTickMs();

        std::lock_guard<std::mutex> guard(stateMutex);
        latest = next;
        hasLatest = true;
    }
};

HidInputBackend::HidInputBackend(MyHidConfig cfg)
    : impl_(new Impl(cfg)) {
}

HidInputBackend::~HidInputBackend() {
    delete impl_;
    impl_ = nullptr;
}

bool HidInputBackend::start() {
    return impl_->start();
}

void HidInputBackend::stop() {
    impl_->stop();
}

bool HidInputBackend::tryGetLatest(HidOverlayState& out) const {
    return impl_->tryGetLatest(out);
}

void HidInputBackend::setTargetVidPid(uint16_t vid, uint16_t pid) {
    impl_->setTargetVidPid(vid, pid);
}

bool HidInputBackend::autoConfigure(uint16_t vid, uint16_t pid, HidCapabilities* outCaps) {
    return impl_->autoConfigure(vid, pid, outCaps);
}

MyHidConfig HidInputBackend::getConfig() const {
    return impl_->config;
}
