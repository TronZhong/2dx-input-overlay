#pragma once

#include "core/types/hid_types.h"
#include "adapter/my_hid_adapter.h"
#include <windows.h>
#include <hidsdi.h>

namespace axis_parser {

// Parse all axes from a report using dynamic axis mappings
void parseAxes(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState,
    std::vector<MyHidAdapter::AxisTrack>& axisTracks);

} // namespace axis_parser