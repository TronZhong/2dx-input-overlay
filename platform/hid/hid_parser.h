#pragma once

#include <windows.h>
#include <hidsdi.h>
#include <vector>
#include <cstdint>

// Thin wrappers around HidP_* APIs for parsing input reports

namespace hid_parser {

// Get all active button usages from a report
bool getActiveButtonUsages(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    const uint8_t* report,
    ULONG reportSize,
    std::vector<USAGE>& outUsages);

// Get a single usage value (axis/scalar)
bool getUsageValue(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    const uint8_t* report,
    ULONG reportSize,
    LONG& outValue);

} // namespace hid_parser