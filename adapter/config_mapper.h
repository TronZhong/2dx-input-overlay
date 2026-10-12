#pragma once

#include "core/types/hid_types.h"
#include "core/capabilities/hid_capabilities.h"

namespace config_mapper {

// Map HidCapabilities to MyHidConfig dynamic mappings
// Returns true if at least one button or axis was mapped
bool mapCapabilitiesToConfig(const HidCapabilities& caps, MyHidConfig& config);

} // namespace config_mapper