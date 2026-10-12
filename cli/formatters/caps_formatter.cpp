#include "caps_formatter.h"
#include "core/capabilities/hid_capabilities.h"

#include <iostream>

namespace caps_formatter {

void printCaps(const HidCapabilities& caps) {
    std::cout << "\n=== Detected Capabilities ===\n";
    std::cout << "Buttons (" << caps.buttons.size() << "):\n";
    for (const auto& b : caps.buttons) {
        std::cout << "  usage=0x" << std::hex << b.usage << std::dec
                  << " link=" << b.linkCollection
                  << " name='" << b.name << "'\n";
    }
    std::cout << "Values / axes (" << caps.values.size() << "):\n";
    for (const auto& v : caps.values) {
        std::cout << "  page=0x" << std::hex << v.usagePage << std::dec
                  << " usage=0x" << std::hex << v.usage << std::dec
                  << " link=" << v.linkCollection
                  << " range=[" << v.logicalMin << ", " << v.logicalMax << "]"
                  << (v.isRange ? " (range)" : " (bit)")
                  << " name='" << v.name << "'\n";
    }
    std::cout << "=============================\n\n";
}

} // namespace caps_formatter