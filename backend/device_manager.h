#pragma once

#include "core/types/hid_types.h"
#include "core/capabilities/hid_capabilities.h"
#include <map>
#include <vector>
#include <string>

struct TargetHidDevice {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::string name;
    std::vector<uint8_t> preparsed;
    std::vector<USAGE> buttonUsages;
    std::vector<std::pair<USHORT, USAGE>> axisUsages;
};

class DeviceManager {
public:
    explicit DeviceManager(const MyHidConfig& config);
    ~DeviceManager();

    void setConfig(const MyHidConfig& config);
    void scanTargetDevices();
    bool tryAddTargetDevice(HANDLE handle);
    void removeTargetDevice(HANDLE handle);
    
    const std::map<HANDLE, TargetHidDevice>& devices() const { return devices_; }
    void clear() { devices_.clear(); }
    
    // Index capabilities for a device (called after preparsed data loaded)
    void indexDeviceCapabilities(TargetHidDevice& dev);
    
    // Check ownership
    bool ownsButton(const TargetHidDevice& dev, USAGE usage) const;
    bool ownsAxis(const TargetHidDevice& dev, USAGE usagePage, USAGE usage) const;

private:
    MyHidConfig config_;
    std::map<HANDLE, TargetHidDevice> devices_;
    
    bool loadPreparsedData(HANDLE handle, std::vector<uint8_t>& out);
    std::string getRawInputDeviceName(HANDLE handle);
};