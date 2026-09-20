#include <iostream>
#include <string>
#include <vector>
#include <optional>

#include <windows.h>
#include <conio.h>

#include "hid_input_backend.h"
#include "my_hid_adapter.h"
#include "DeviceEnumerator.h"
#include "DeviceProfile.h"

static void printDeviceList(const std::vector<DeviceInfo>& devices) {
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

static void printConfig(const MyHidConfig& config) {
    std::cout << "\n=== Current Config ===\n";
    std::cout << "VID=0x" << std::hex << config.vid << std::dec
              << " PID=0x" << std::hex << config.pid << std::dec << "\n";
    std::cout << "Buttons (legacy): page=0x" << std::hex << config.buttonUsagePage << std::dec
              << " usages=[";
    for (int i = 1; i <= 7; ++i) {
        USAGE u = 0;
        switch (i) {
            case 1: u = config.button_01Usage; break;
            case 2: u = config.button_02Usage; break;
            case 3: u = config.button_03Usage; break;
            case 4: u = config.button_04Usage; break;
            case 5: u = config.button_05Usage; break;
            case 6: u = config.button_06Usage; break;
            case 7: u = config.button_07Usage; break;
        }
        std::cout << "0x" << std::hex << u << std::dec << (i < 7 ? ", " : "");
    }
    std::cout << "]\n";
    std::cout << "Axis (legacy): page=0x" << std::hex << config.axisUsagePage << std::dec
              << " usage=0x" << std::hex << config.xUsage << std::dec
              << " range=[" << config.xLogicalMin << ", " << config.xLogicalMax << "]\n";
    std::cout << "xIdleTimeoutMs=" << config.xIdleTimeoutMs << "\n";

    if (!config.buttons.empty()) {
        std::cout << "Dynamic buttons (" << config.buttons.size() << "):\n";
        for (const auto& btn : config.buttons) {
            std::cout << "  [" << btn.index << "] page=0x" << std::hex << btn.usagePage << std::dec
                      << " usage=0x" << std::hex << btn.usage << std::dec
                      << " link=" << btn.linkCollection
                      << " name='" << btn.name << "'\n";
        }
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
    }
    std::cout << "========================\n\n";
}

int main() {
    std::cout << "2DX Input Overlay - Standalone Test\n";
    std::cout << "Commands: l=list devices, s=N=select device, p=print config, w=write profile, q=quit\n\n";

    DeviceProfileManager profileMgr;

    // Initial device list
    auto devices = DeviceEnumerator::enumerateAll();
    printDeviceList(devices);

    // Default config (PhoenixWan+)
    MyHidConfig config {};
    config.vid = 0x034C;
    config.pid = 0x0368;

    // Try to load profile for default device
    auto profile = profileMgr.loadProfile(config.vid, config.pid);
    if (profile) {
        config = profile->config;
        std::cout << "Loaded profile for VID=0x" << std::hex << config.vid << std::dec
                  << " PID=0x" << std::hex << config.pid << std::dec
                  << " (" << profile->name << ")\n";
    }

    HidInputBackend backend(config);
    if (!backend.start()) {
        std::cerr << "Failed to start HID input backend.\n";
        return 1;
    }

    std::cout << "Backend started. Monitoring VID=0x" << std::hex << config.vid << std::dec
              << " PID=0x" << std::hex << config.pid << std::dec << "\n";
    std::cout << "Enter command> ";

    HidOverlayState last {};
    bool hasLast = false;

    std::string line;
    while (true) {
        // Poll backend state
        HidOverlayState state {};
        if (backend.tryGetLatest(state)) {
            bool changed = !hasLast
                || state.connected != last.connected
                || state.button01 != last.button01
                || state.button02 != last.button02
                || state.button03 != last.button03
                || state.button04 != last.button04
                || state.button05 != last.button05
                || state.button06 != last.button06
                || state.button07 != last.button07
                || state.xDirection != last.xDirection;

            if (changed) {
                std::cout << "\r[HID] connected=" << (state.connected ? 1 : 0)
                          << " b1=" << (state.button01 ? 1 : 0)
                          << " b2=" << (state.button02 ? 1 : 0)
                          << " b3=" << (state.button03 ? 1 : 0)
                          << " b4=" << (state.button04 ? 1 : 0)
                          << " b5=" << (state.button05 ? 1 : 0)
                          << " b6=" << (state.button06 ? 1 : 0)
                          << " b7=" << (state.button07 ? 1 : 0)
                          << " dir=" << state.xDirection << "        \n";
                std::cout << "Enter command> ";
                last = state;
                hasLast = true;
            }
        }

        // Non-blocking console input check
        if (_kbhit()) {
            std::getline(std::cin, line);
            if (line.empty()) {
                std::cout << "Enter command> ";
                continue;
            }

            char cmd = line[0];
            if (cmd == 'l' || cmd == 'L') {
                devices = DeviceEnumerator::enumerateAll();
                printDeviceList(devices);
            } else if (cmd == 's' || cmd == 'S') {
                // s N or s=N
                size_t idx = 0;
                if (line.size() > 1) {
                    if (line[1] == '=') idx = std::stoul(line.substr(2));
                    else idx = std::stoul(line.substr(1));
                }
                if (idx < devices.size()) {
                    uint16_t newVid = devices[idx].vid;
                    uint16_t newPid = devices[idx].pid;
                    std::cout << "Switching to device [" << idx << "] VID=0x" << std::hex << newVid << std::dec
                              << " PID=0x" << std::hex << newPid << std::dec << "\n";

                    // Try to load profile for new device
                    auto newProfile = profileMgr.loadProfile(newVid, newPid);
                    if (newProfile) {
                        config = newProfile->config;
                        std::cout << "Loaded profile: " << newProfile->name << "\n";
                    } else {
                        config.vid = newVid;
                        config.pid = newPid;
                        std::cout << "No profile found, using defaults.\n";
                    }

                    // Use setTargetVidPid for hot-swap without restarting thread
                    backend.setTargetVidPid(config.vid, config.pid);
                } else {
                    std::cout << "Invalid index. Use 'l' to list devices.\n";
                }
            } else if (cmd == 'p' || cmd == 'P') {
                printConfig(config);
            } else if (cmd == 'w' || cmd == 'W') {
                // w [name] - save current config as profile
                std::string name;
                if (line.size() > 1) {
                    if (line[1] == ' ') name = line.substr(2);
                    else if (line[1] == '=') name = line.substr(2);
                }
                if (name.empty()) {
                    char defaultName[64];
                    sprintf_s(defaultName, "Device_%04X_%04X", config.vid, config.pid);
                    name = defaultName;
                }
                if (profileMgr.saveProfile(config, name)) {
                    std::cout << "Profile saved for VID=0x" << std::hex << config.vid << std::dec
                              << " PID=0x" << std::hex << config.pid << std::dec
                              << " as '" << name << "'\n";
                } else {
                    std::cerr << "Failed to save profile.\n";
                }
            } else if (cmd == 'q' || cmd == 'Q') {
                std::cout << "Quitting...\n";
                break;
            } else {
                std::cout << "Unknown command. Use: l, s=N, p, w [name], q\n";
            }
            std::cout << "Enter command> ";
        }

        Sleep(5);
    }

    backend.stop();
    return 0;
}