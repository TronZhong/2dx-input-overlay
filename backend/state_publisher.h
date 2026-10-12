#pragma once

#include "core/types/hid_types.h"
#include <mutex>

class StatePublisher {
public:
    StatePublisher() = default;
    ~StatePublisher() = default;

    bool tryGetLatest(HidOverlayState& out) const;
    void publishState(const MyHidState& state, const MyHidConfig& config);
    void publishDisconnectedState();

private:
    mutable std::mutex mutex_;
    HidOverlayState latest_;
    bool has_latest_ = false;
};