#include "select_cmd.h"
#include "cli/commands/command.h"
#include "platform/enumerator/device_enumerator.h"
#include "cli/formatters/device_formatter.h"
#include "cli/formatters/caps_formatter.h"
#include "core/capabilities/hid_capabilities.h"
#include <iostream>
#include <cstdint>

bool SelectCommand::execute(CliContext& ctx, const std::string& args) {
    if (args.empty()) {
        std::cout << "Usage: s N (select device by index)\n";
        return true;
    }

    size_t idx = 0;
    try {
        idx = std::stoul(args);
    } catch (...) {
        std::cout << "Invalid index: " << args << "\n";
        return true;
    }

    if (idx >= ctx.devices.size()) {
        std::cout << "Invalid index. Use 'l' to list devices.\n";
        return true;
    }

    uint16_t newVid = ctx.devices[idx].vid;
    uint16_t newPid = ctx.devices[idx].pid;
    std::cout << "Switching to device [" << idx << "] VID=0x" << std::hex << newVid << std::dec
              << " PID=0x" << std::hex << newPid << std::dec << "\n";

    // Refresh device list to show collections
    ctx.devices = DeviceEnumerator::enumerateAll();
    device_formatter::printDeviceCollections(ctx.devices, newVid, newPid);

    // Try to load profile
    auto profile = ctx.profileMgr.loadProfile(newVid, newPid);
    if (profile) {
        ctx.config = profile->config;
        std::cout << "Loaded profile: " << profile->name << "\n";
        ctx.backend.setTargetVidPid(ctx.config.vid, ctx.config.pid);
    } else {
        std::cout << "No profile found, auto-detecting capabilities...\n";
        HidCapabilities caps;
        if (ctx.backend.autoConfigure(newVid, newPid, &caps)) {
            ctx.config = ctx.backend.getConfig();
            caps_formatter::printCaps(caps);
            std::cout << "Auto-detect complete, monitoring device.\n";
        } else {
            std::cout << "Auto-detect failed for this device; using defaults.\n";
            ctx.config.vid = newVid;
            ctx.config.pid = newPid;
            ctx.backend.setTargetVidPid(ctx.config.vid, ctx.config.pid);
        }
    }

    return true;
}