#include "device_manager.h"

#include <windows.h>
#include <hidsdi.h>
#include "core/capabilities/hid_capabilities.h"
#include "platform/raw_input/raw_input_parser.h"
#include "platform/hid/hid_parser.h"

DeviceManager::DeviceManager(const MyHidConfig& config)
    : config_(config) {}

DeviceManager::~DeviceManager() {
    devices_.clear();
}

void DeviceManager::setConfig(const MyHidConfig& config) {
    config_ = config;
    devices_.clear();
}

void DeviceManager::scanTargetDevices() {
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || count == 0) {
        return;
    }

    std::vector<RAWINPUTDEVICELIST> list(count);
    if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) {
        return;
    }

    for (const auto& item : list) {
        if (item.dwType != RIM_TYPEHID) continue;
        tryAddTargetDevice(item.hDevice);
    }
}

bool DeviceManager::tryAddTargetDevice(HANDLE handle) {
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;
    if (devices_.find(handle) != devices_.end()) return true;

    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT infoSize = info.cbSize;
    if (GetRawInputDeviceInfo(handle, RIDI_DEVICEINFO, &info, &infoSize) == static_cast<UINT>(-1)) {
        return false;
    }

    if (info.dwType != RIM_TYPEHID) return false;

    uint16_t vid = static_cast<uint16_t>(info.hid.dwVendorId);
    uint16_t pid = static_cast<uint16_t>(info.hid.dwProductId);
    if (!config_.matches(vid, pid)) return false;

    TargetHidDevice target;
    target.handle = handle;
    target.name = RawInputParser::getDeviceName(handle);
    
    if (!loadPreparsedData(handle, target.preparsed)) {
        return false;
    }

    indexDeviceCapabilities(target);
    devices_[handle] = std::move(target);
    return true;
}

void DeviceManager::removeTargetDevice(HANDLE handle) {
    devices_.erase(handle);
}

void DeviceManager::indexDeviceCapabilities(TargetHidDevice& dev) {
    dev.buttonUsages.clear();
    dev.axisUsages.clear();
    if (dev.preparsed.empty()) return;

    HidCapabilities caps = CapabilityInspector::enumerate(
        reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(dev.preparsed.data())));
    
    if (!caps.valid) return;

    for (const auto& b : caps.buttons) {
        dev.buttonUsages.push_back(b.usage);
    }
    for (const auto& v : caps.values) {
        if (v.isRange) {
            dev.axisUsages.emplace_back(static_cast<USHORT>(v.usagePage), v.usage);
        }
    }
}

bool DeviceManager::ownsButton(const TargetHidDevice& dev, USAGE usage) const {
    for (USAGE u : dev.buttonUsages) {
        if (u == usage) return true;
    }
    return false;
}

bool DeviceManager::ownsAxis(const TargetHidDevice& dev, USAGE usagePage, USAGE usage) const {
    for (const auto& p : dev.axisUsages) {
        if (p.first == static_cast<USHORT>(usagePage) && p.second == usage) return true;
    }
    return false;
}

bool DeviceManager::loadPreparsedData(HANDLE handle, std::vector<uint8_t>& out) {
    return RawInputParser::getPreparsedData(handle, out);
}

std::string DeviceManager::getRawInputDeviceName(HANDLE handle) {
    return RawInputParser::getDeviceName(handle);
}