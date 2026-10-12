#pragma once

#include "core/types/hid_types.h"
#include <vector>

namespace device_formatter {

void printDeviceList(const std::vector<DeviceInfo>& devices);
void printDeviceCollections(const std::vector<DeviceInfo>& all, uint16_t vid, uint16_t pid);

} // namespace device_formatter