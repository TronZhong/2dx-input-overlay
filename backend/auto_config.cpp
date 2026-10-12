#include "auto_config.h"

#include "core/capabilities/hid_capabilities.h"

bool AutoConfig::configure(
    uint16_t vid,
    uint16_t pid,
    DeviceManager& deviceManager,
    MyHidAdapter& adapter,
    HidCapabilities* outCaps) {

    // Scan for target devices
    deviceManager.scanTargetDevices();
    const auto& devices = deviceManager.devices();

    // Merge capabilities across all collections of the target device
    HidCapabilities merged;
    bool any = false;

    for (const auto& kv : devices) {
        const auto& dev = kv.second;
        if (dev.preparsed.empty()) continue;

        HidCapabilities caps = CapabilityInspector::enumerate(
            reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<uint8_t*>(dev.preparsed.data())));
        
        if (!caps.valid) continue;
        any = true;

        // Append buttons, deduplicated by (usage, linkCollection)
        for (const auto& b : caps.buttons) {
            bool dup = false;
            for (const auto& m : merged.buttons) {
                if (m.usage == b.usage && m.linkCollection == b.linkCollection) {
                    dup = true;
                    break;
                }
            }
            if (!dup) merged.buttons.push_back(b);
        }

        // Append values/axes, deduplicated by (usagePage, usage, linkCollection)
        for (const auto& v : caps.values) {
            bool dup = false;
            for (const auto& m : merged.values) {
                if (m.usagePage == v.usagePage && m.usage == v.usage
                    && m.linkCollection == v.linkCollection) {
                    dup = true;
                    break;
                }
            }
            if (!dup) merged.values.push_back(v);
        }
    }

    if (!any) return false;
    merged.valid = true;

    // Apply detected mappings to adapter
    if (adapter.autoDetect(merged)) {
        // Adapter config updated internally
    }

    if (outCaps) {
        *outCaps = std::move(merged);
    }
    return true;
}