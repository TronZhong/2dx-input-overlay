#include "HidCapabilities.h"

#include <sstream>

namespace {

// Human-friendly label for a generic-desktop value usage id.
const char* genericDesktopName(USAGE usage) {
    switch (usage) {
        case 0x30: return "X";
        case 0x31: return "Y";
        case 0x32: return "Z";
        case 0x33: return "Rx";
        case 0x34: return "Ry";
        case 0x35: return "Rz";
        case 0x36: return "Slider";
        case 0x37: return "Dial";
        case 0x38: return "Wheel";
        case 0x39: return "Hat";
        default: return nullptr;
    }
}

std::string valueName(USAGE usagePage, USAGE usage) {
    if (usagePage == 0x01) {
        if (const char* n = genericDesktopName(usage)) {
            return n;
        }
    }
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << usage;
    return os.str();
}

} // namespace

HidCapabilities CapabilityInspector::enumerate(PHIDP_PREPARSED_DATA preparsed) {
    HidCapabilities result;
    if (!preparsed) {
        return result;
    }

    // --- Buttons ---
    USHORT buttonCapsLength = 0;
    if (HidP_GetButtonCaps(HidP_Input, nullptr, &buttonCapsLength, preparsed) == HIDP_STATUS_SUCCESS
        && buttonCapsLength > 0) {
        std::vector<HIDP_BUTTON_CAPS> caps(buttonCapsLength);
        if (HidP_GetButtonCaps(HidP_Input, caps.data(), &buttonCapsLength, preparsed) == HIDP_STATUS_SUCCESS) {
            for (const auto& c : caps) {
                if (c.IsRange) {
                    // Expand a button range into individual usages.
                    for (USAGE u = c.Range.UsageMin; u >= c.Range.UsageMin && u <= c.Range.UsageMax; ++u) {
                        if (u == 0) {
                            break;
                        }
                        DetectedButton b;
                        b.usage = u;
                        b.linkCollection = c.LinkCollection;
                        b.name = "Btn " + std::to_string(u);
                        result.buttons.push_back(b);
                    }
                } else {
                    DetectedButton b;
                    b.usage = c.NotRange.Usage;
                    b.linkCollection = c.LinkCollection;
                    b.name = "Btn " + std::to_string(c.NotRange.Usage);
                    result.buttons.push_back(b);
                }
            }
        }
    }

    // --- Values (axes / scalar usages) ---
    USHORT valueCapsLength = 0;
    if (HidP_GetValueCaps(HidP_Input, nullptr, &valueCapsLength, preparsed) == HIDP_STATUS_SUCCESS
        && valueCapsLength > 0) {
        std::vector<HIDP_VALUE_CAPS> caps(valueCapsLength);
        if (HidP_GetValueCaps(HidP_Input, caps.data(), &valueCapsLength, preparsed) == HIDP_STATUS_SUCCESS) {
            for (const auto& c : caps) {
                if (c.IsRange) {
                    for (USAGE u = c.Range.UsageMin; u >= c.Range.UsageMin && u <= c.Range.UsageMax; ++u) {
                        if (u == 0) {
                            break;
                        }
                        DetectedValue v;
                        v.usagePage = c.UsagePage;
                        v.usage = u;
                        v.linkCollection = c.LinkCollection;
                        v.logicalMin = c.LogicalMin;
                        v.logicalMax = c.LogicalMax;
                        v.isRange = (c.LogicalMax > c.LogicalMin);
                        v.name = valueName(c.UsagePage, u);
                        result.values.push_back(v);
                    }
                } else {
                    DetectedValue v;
                    v.usagePage = c.UsagePage;
                    v.usage = c.NotRange.Usage;
                    v.linkCollection = c.LinkCollection;
                    v.logicalMin = c.LogicalMin;
                    v.logicalMax = c.LogicalMax;
                    v.isRange = (c.LogicalMax > c.LogicalMin);
                    v.name = valueName(c.UsagePage, c.NotRange.Usage);
                    result.values.push_back(v);
                }
            }
        }
    }

    result.valid = true;
    return result;
}
