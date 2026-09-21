#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iomanip>
#include <cstdio>

#include <windows.h>
#include <conio.h>

#include "hid_input_backend.h"
#include "my_hid_adapter.h"
#include "HidCapabilities.h"
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

static void printCaps(const HidCapabilities& caps) {
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

static std::string fmtTimestamp(uint64_t tickMs) {
    (void)tickMs;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[48];
    snprintf(buf, sizeof(buf), "[%02u:%02u:%02u.%03u]",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

static bool stateChanged(const HidOverlayState& a, const HidOverlayState& b) {
    if (a.connected != b.connected) return true;
    if (a.xNorm != b.xNorm || a.xDirection != b.xDirection) return true;
    if (a.button01 != b.button01 || a.button02 != b.button02 || a.button03 != b.button03 ||
        a.button04 != b.button04 || a.button05 != b.button05 || a.button06 != b.button06 ||
        a.button07 != b.button07) return true;
    if (a.buttons != b.buttons || a.axesNorm != b.axesNorm) return true;
    if (a.axesRaw != b.axesRaw || a.axesDir != b.axesDir) return true;
    return false;
}

static std::string formatLiveLine(const MyHidConfig& cfg, const HidOverlayState& s) {
    std::ostringstream os;
    os << fmtTimestamp(s.tickMs) << " connected=" << (s.connected ? 1 : 0) << "\n";

    if (!cfg.buttons.empty() && s.buttons.size() == cfg.buttons.size()) {
        os << "  Btn:";
        for (size_t i = 0; i < s.buttons.size(); ++i) {
            os << " [" << i << "]" << (s.buttons[i] ? 1 : 0)
               << "(" << cfg.buttons[i].name << ")";
        }
    } else {
        os << "  Btn(legacy): " << (s.button01 ? 1 : 0) << " " << (s.button02 ? 1 : 0)
           << " " << (s.button03 ? 1 : 0) << " " << (s.button04 ? 1 : 0)
           << " " << (s.button05 ? 1 : 0) << " " << (s.button06 ? 1 : 0)
           << " " << (s.button07 ? 1 : 0);
    }

    os << "\n  Axis:";
    if (!cfg.axes.empty() && s.axesNorm.size() == cfg.axes.size()) {
        for (size_t i = 0; i < s.axesNorm.size(); ++i) {
            char arrow = s.axesDir[i] > 0 ? '^' : (s.axesDir[i] < 0 ? 'v' : '-');
            os << " [" << i << "]=" << std::fixed << std::setprecision(2) << s.axesNorm[i]
               << "(" << s.axesRaw[i] << ")" << arrow
               << "(" << cfg.axes[i].name << ")";
        }
    } else {
        char arrow = s.xDirection > 0 ? '^' : (s.xDirection < 0 ? 'v' : '-');
        os << " X=" << std::fixed << std::setprecision(2) << s.xNorm << arrow;
    }

    return os.str();
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
            // Multi-line scrolling output. Print when not typing and when the
            // semantic state changed, so the live feed stays readable.
            if (!_kbhit() && (!hasLast || stateChanged(state, last))) {
                std::cout << formatLiveLine(config, state) << "\n";
                std::cout << "Enter command> ";
                std::cout.flush();
            }
            last = state;
            hasLast = true;
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

                    // Try to load profile for new device; if none, auto-detect.
                    auto newProfile = profileMgr.loadProfile(newVid, newPid);
                    if (newProfile) {
                        config = newProfile->config;
                        std::cout << "Loaded profile: " << newProfile->name << "\n";
                        backend.setTargetVidPid(config.vid, config.pid);
                    } else {
                        std::cout << "No profile found, auto-detecting capabilities...\n";
                        HidCapabilities caps;
                        if (backend.autoConfigure(newVid, newPid, &caps)) {
                            config = backend.getConfig();
                            printCaps(caps);
                            std::cout << "Auto-detect complete, monitoring device.\n";
                        } else {
                            std::cout << "Auto-detect failed for this device; using defaults.\n";
                            config.vid = newVid;
                            config.pid = newPid;
                            backend.setTargetVidPid(config.vid, config.pid);
                        }
                    }
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