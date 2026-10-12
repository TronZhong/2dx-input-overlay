#include "live_formatter.h"
#include "core/types/hid_types.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <windows.h>

namespace live_formatter {

std::string fmtTimestamp(uint64_t tickMs) {
    (void)tickMs;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[48];
    snprintf(buf, sizeof(buf), "[%02u:%02u:%02u.%03u]",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

bool stateChanged(const HidOverlayState& a, const HidOverlayState& b) {
    if (a.connected != b.connected) return true;
    if (a.buttons != b.buttons) return true;
    if (a.axesNorm != b.axesNorm) return true;
    if (a.axesRaw != b.axesRaw) return true;
    if (a.axesDir != b.axesDir) return true;
    return false;
}

std::string formatLiveLine(const MyHidConfig& cfg, const HidOverlayState& s) {
    std::ostringstream os;
    os << fmtTimestamp(s.tickMs) << " connected=" << (s.connected ? 1 : 0) << "\n";

    // Buttons
    if (!cfg.buttons.empty() && s.buttons.size() == cfg.buttons.size()) {
        os << "  Btn:";
        for (size_t i = 0; i < s.buttons.size(); ++i) {
            os << " [" << i << "]" << (s.buttons[i] ? 1 : 0)
               << "(" << cfg.buttons[i].name << ")";
        }
    } else {
        os << "  Btn: (no dynamic mapping)";
    }

    // Axes
    os << "\n  Axis:";
    if (!cfg.axes.empty() && s.axesNorm.size() == cfg.axes.size()) {
        for (size_t i = 0; i < s.axesNorm.size(); ++i) {
            char arrow = s.axesDir[i] > 0 ? '^' : (s.axesDir[i] < 0 ? 'v' : '-');
            os << " [" << i << "]=" << std::fixed << std::setprecision(2) << s.axesNorm[i]
               << "(" << s.axesRaw[i] << ")" << arrow
               << "(" << cfg.axes[i].name << ")";
        }
    } else {
        os << " (no dynamic mapping)";
    }

    return os.str();
}

} // namespace live_formatter