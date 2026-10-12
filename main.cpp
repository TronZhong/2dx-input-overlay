#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <conio.h>

#include <windows.h>

#include "backend/hid_input_backend.h"
#include "core/profiles/device_profile.h"
#include "platform/enumerator/device_enumerator.h"
#include "cli/commands/command.h"
#include "cli/commands/list_cmd.h"
#include "cli/commands/select_cmd.h"
#include "cli/commands/print_cmd.h"
#include "cli/commands/write_cmd.h"
#include "cli/commands/quit_cmd.h"
#include "cli/formatters/device_formatter.h"
#include "cli/formatters/live_formatter.h"

int main() {
    std::cout << "2DX Input Overlay - Standalone Test\n";
    std::cout << "Commands: l=list devices, s N=select device, p=print config, w [name]=write profile, q=quit\n\n";

    DeviceProfileManager profileMgr;

    // Initial device list
    MyHidConfig defaultConfig{};
    defaultConfig.vid = 0x034C;
    defaultConfig.pid = 0x0368;

    HidInputBackend backend(defaultConfig);
    
    CliContext ctx{backend, profileMgr, {}, defaultConfig, true};
    ctx.devices = DeviceEnumerator::enumerateAll();
    device_formatter::printDeviceList(ctx.devices);

    // Try to load profile for default device
    auto profile = profileMgr.loadProfile(ctx.config.vid, ctx.config.pid);
    if (profile) {
        ctx.config = profile->config;
        std::cout << "Loaded profile for VID=0x" << std::hex << ctx.config.vid << std::dec
                  << " PID=0x" << std::hex << ctx.config.pid << std::dec
                  << " (" << profile->name << ")\n";
    }

    if (!backend.start()) {
        std::cerr << "Failed to start HID input backend.\n";
        return 1;
    }

    std::cout << "Backend started. Monitoring VID=0x" << std::hex << ctx.config.vid << std::dec
              << " PID=0x" << std::hex << ctx.config.pid << std::dec << "\n";
    std::cout << "Enter command> ";

    // Register commands
    std::map<char, std::unique_ptr<Command>> commands;
    commands['l'] = std::make_unique<ListCommand>();
    commands['s'] = std::make_unique<SelectCommand>();
    commands['p'] = std::make_unique<PrintCommand>();
    commands['w'] = std::make_unique<WriteCommand>();
    commands['q'] = std::make_unique<QuitCommand>();

    HidOverlayState last{};
    bool hasLast = false;

    while (ctx.running) {
        // Poll backend state
        HidOverlayState state{};
        if (backend.tryGetLatest(state)) {
            if (!_kbhit() && (!hasLast || live_formatter::stateChanged(state, last))) {
                std::cout << live_formatter::formatLiveLine(ctx.config, state) << "\n";
                std::cout << "Enter command> ";
                std::cout.flush();
            }
            last = state;
            hasLast = true;
        }

        // Non-blocking console input
        if (_kbhit()) {
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) {
                std::cout << "Enter command> ";
                continue;
            }

            char cmd = line[0];
            std::string args = line.size() > 1 ? line.substr(1) : "";
            
            if (args.size() > 0 && args[0] == ' ') {
                args = args.substr(1);
            }

            auto it = commands.find(cmd);
            if (it != commands.end()) {
                it->second->execute(ctx, args);
            } else {
                std::cout << "Unknown command. Use: l, s N, p, w [name], q\n";
            }
            std::cout << "Enter command> ";
        }

        Sleep(5);
    }

    backend.stop();
    return 0;
}