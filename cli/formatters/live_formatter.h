#pragma once

#include "core/types/hid_types.h"
#include <string>

namespace live_formatter {

std::string fmtTimestamp(uint64_t tickMs);
bool stateChanged(const HidOverlayState& a, const HidOverlayState& b);
std::string formatLiveLine(const MyHidConfig& cfg, const HidOverlayState& s);

} // namespace live_formatter