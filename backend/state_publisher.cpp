#include "state_publisher.h"

#include <windows.h>

namespace {
uint64_t currentTickMs() {
#if (_WIN32_WINNT >= 0x0600)
    return GetTickCount64();
#else
    return static_cast<uint64_t>(GetTickCount());
#endif
}
} // namespace

bool StatePublisher::tryGetLatest(HidOverlayState& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_latest_) return false;
    out = latest_;
    return true;
}

void StatePublisher::publishState(const MyHidState& state, const MyHidConfig& config) {
    HidOverlayState next;
    next.connected = state.connected;
    next.tickMs = currentTickMs();
    
    // Copy dynamic arrays
    next.buttons = state.dynamicButtons;
    next.axesNorm = state.dynamicAxesNorm;
    next.axesRaw = state.dynamicAxesRaw;
    next.axesDir = state.dynamicAxesDir;

    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = next;
    has_latest_ = true;
}

void StatePublisher::publishDisconnectedState() {
    HidOverlayState next;
    next.connected = false;
    next.tickMs = currentTickMs();
    // Vectors remain empty

    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = next;
    has_latest_ = true;
}