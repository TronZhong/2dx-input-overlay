#pragma once

#include <string>
#include <windows.h>
#include <hidsdi.h>

namespace hid_usage_names {

const char* genericDesktopName(USAGE usage);
std::string valueName(USAGE usagePage, USAGE usage);

} // namespace hid_usage_names