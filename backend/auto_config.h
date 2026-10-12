#pragma once

#include "core/types/hid_types.h"
#include "core/capabilities/hid_capabilities.h"
#include "backend/device_manager.h"
#include "adapter/my_hid_adapter.h"

class AutoConfig {
public:
    // Auto-detect device capabilities and apply to adapter
    // Returns true if at least one collection had valid capabilities
    static bool configure(
        uint16_t vid,
        uint16_t pid,
        DeviceManager& deviceManager,
        MyHidAdapter& adapter,
        HidCapabilities* outCaps = nullptr);
};