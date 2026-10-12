#pragma once

#include "core/types/hid_types.h"
#include "core/capabilities/hid_capabilities.h"

class MyHidAdapter {
public:
    explicit MyHidAdapter(MyHidConfig cfg = {}) : cfg_(std::move(cfg)) {}

    // Per-axis direction tracking state (mutated under const updateFromReport)
    struct AxisTrack {
        bool hasPrev = false;
        LONG prevRaw = 0;
        int lastDir = 0;
        uint64_t lastMoveTickMs = 0;
    };

    bool matches(uint16_t vid, uint16_t pid) const;

    // Parse one HID input report using dynamic mappings
    bool updateFromReport(
        PHIDP_PREPARSED_DATA preparsed,
        const uint8_t* rawData,
        size_t rawSize,
        MyHidState& outState) const;

    // Auto-generate dynamic button/axis mappings from detected capabilities
    bool autoDetect(const HidCapabilities& caps);

    // Read the adapter's current config (including auto-detected mappings)
    const MyHidConfig& config() const { return cfg_; }

private:
    MyHidConfig cfg_;
    mutable std::vector<AxisTrack> axisTracks_;

    static float normalizeToUnit(LONG value, LONG logicalMin, LONG logicalMax);
    int trackAxisDirection(size_t index, LONG raw, uint64_t now) const;
};