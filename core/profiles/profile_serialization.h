#pragma once

#include "core/profiles/device_profile.h"
#include "core/types/hid_types.h"
#include <nlohmann/json.hpp>

namespace profile_serialization {

using json = nlohmann::json;

json buttonMappingToJson(const ButtonMapping& btn);
json axisMappingToJson(const AxisMapping& axis);
ButtonMapping buttonMappingFromJson(const json& j);
AxisMapping axisMappingFromJson(const json& j);

json configToJson(const MyHidConfig& cfg);
MyHidConfig configFromJson(const json& j);

} // namespace profile_serialization