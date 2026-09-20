#include "DeviceEnumerator.h"

#include <windows.h>
#include <hidsdi.h>
#include <vector>
#include <string>

namespace {

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return {};
    std::string result(static_cast<size_t>(sizeNeeded), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}

std::wstring getRawInputDeviceNameW(HANDLE deviceHandle) {
    UINT chars = 0;
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, nullptr, &chars) == static_cast<UINT>(-1) || chars == 0) {
        return L"";
    }
    std::wstring name(chars, L'\0');
    if (GetRawInputDeviceInfoW(deviceHandle, RIDI_DEVICENAME, &name[0], &chars) == static_cast<UINT>(-1)) {
        return L"";
    }
    if (!name.empty() && name.back() == L'\0') name.pop_back();
    return name;
}

} // namespace

std::vector<DeviceInfo> DeviceEnumerator::enumerateAll() {
    std::vector<DeviceInfo> result;

    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count == 0) {
        return result;
    }

    std::vector<RAWINPUTDEVICELIST> list(count);
    if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
        return result;
    }

    for (const auto& item : list) {
        if (item.dwType != RIM_TYPEHID) continue;

        RID_DEVICE_INFO info{};
        info.cbSize = sizeof(info);
        UINT infoSize = info.cbSize;
        if (GetRawInputDeviceInfo(item.hDevice, RIDI_DEVICEINFO, &info, &infoSize) == static_cast<UINT>(-1)) {
            continue;
        }

        if (info.dwType != RIM_TYPEHID) continue;

        DeviceInfo dev;
        dev.handle = item.hDevice;
        dev.vid = static_cast<uint16_t>(info.hid.dwVendorId);
        dev.pid = static_cast<uint16_t>(info.hid.dwProductId);
        dev.name = getRawInputDeviceNameW(item.hDevice);
        dev.usagePage = info.hid.usUsagePage;
        dev.usage = info.hid.usUsage;

        result.push_back(std::move(dev));
    }

    return result;
}