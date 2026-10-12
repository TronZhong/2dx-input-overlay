#include "config_formatter.h"
#include "core/types/hid_types.h"

#include <iostream>
#include <iomanip>

namespace config_formatter {

void printConfig(const MyHidConfig& config) {
    std::cout << "\n=== Current Config ===\n";
    std::cout << "VID=0x" << std::hex << config.vid << std::dec
              << " PID=0x" << std::hex << config.pid << std::dec << "\n";

    if (!config.buttons.empty()) {
        std::cout << "Dynamic buttons (" << config.buttons.size() << "):\n";
        for (const auto& btn : config.buttons) {
            std::cout << "  [" << btn.index << "] page=0x" << std::hex << btn.usagePage << std::dec
                      << " usage=0x" << std::hex << btn.usage << std::dec
                      << " link=" << btn.linkCollection
                      << " name='" << btn.name << "'\n";
        }
    } else {
        std::cout << "Buttons: (none)\n";
    }

    if (!config.axes.empty()) {
        std::cout << "Dynamic axes (" << config.axes.size() << "):\n";
        for (const auto& axis : config.axes) {
            std::cout << "  [" << axis.index << "] page=0x" << std::hex << axis.usagePage << std::dec
                      << " usage=0x" << std::hex << axis.usage << std::dec
                      << " link=" << axis.linkCollection
                      << " range=[" << axis.logicalMin << ", " << axis.logicalMax << "]"
                      << " name='" << axis.name << "'\n";
        }
    } else {
        std::cout << "Axes: (none)\n";
    }
    std::cout << "========================\n\n";
}

} // namespace config_formatter