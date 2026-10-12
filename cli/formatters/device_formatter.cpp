#include "device_formatter.h"
#include "core/types/hid_types.h"

#include <iostream>
#include <iomanip>
#include <cstdio>

namespace device_formatter {

void printDeviceList(const std::vector<DeviceInfo>& devices) {
    std::cout << "\n=== HID Devices ===\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        char vidStr[8], pidStr[8];
        sprintf_s(vidStr, "0x%04X", d.vid);
        sprintf_s(pidStr, "0x%04X", d.pid);
        std::wcout << "  [" << i << "] VID=" << vidStr << " PID=" << pidStr
                   << " UsagePage=0x" << std::hex << d.usagePage << std::dec
                   << " Usage=0x" << std::hex << d.usage << std::dec
                   << " Name=" << d.name.c_str() << "\n";
    }
    std::cout << "===================\n\n";
}

void printDeviceCollections(const std::vector<DeviceInfo>& all, uint16_t vid, uint16_t pid) {
    std::cout << "Collections for VID=0x" << std::hex << vid << std::dec
              << " PID=0x" << std::hex << pid << std::dec << ":\n";
    bool any = false;
    for (const auto& d : all) {
        if (d.vid != vid || d.pid != pid) continue;
        any = true;
        std::cout << "  usagePage=0x" << std::hex << d.usagePage << std::dec
                  << " usage=0x" << std::hex << d.usage << std::dec
                  << "  " << d.name.c_str() << "\n";
    }
    std::cout << (any ? "" : "  (none)\n") << "\n";
}

} // namespace device_formatter