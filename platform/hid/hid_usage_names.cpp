#include "hid_usage_names.h"

#include <sstream>

namespace hid_usage_names {

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

} // namespace hid_usage_names