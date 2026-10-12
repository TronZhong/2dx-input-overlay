#pragma once

#include "core/types/hid_types.h"
#include <vector>

class DeviceEnumerator {
public:
    static std::vector<DeviceInfo> enumerateAll();
};