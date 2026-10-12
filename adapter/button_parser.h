#pragma once

#include "core/types/hid_types.h"
#include <windows.h>
#include <hidsdi.h>

namespace button_parser {

// Parse all buttons from a report using dynamic button mappings
void parseButtons(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState);

} // namespace button_parser