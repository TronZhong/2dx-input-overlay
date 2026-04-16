#include <iostream>

#include <windows.h>

#include "hid_input_backend.h"
#include "my_hid_adapter.h"

int main() {
    // 1) Configure which physical controller we want to listen to.
    MyHidConfig config {};
    config.vid = 0x034C;
    config.pid = 0x0368;

    // 2) Start backend thread. In OBS plugin integration this is typically started
    //    in source create/activate lifecycle.
    HidInputBackend backend(config);
    if (!backend.start()) {
        std::cerr << "Failed to start HID input backend.\n";
        return 1;
    }

    std::cout << "HID backend started. Press Ctrl+C to stop.\n";

    HidOverlayState last {};
    bool hasLast = false;

    // 3) Demo consumer loop: poll latest snapshot and print only when meaningful
    //    fields changed. Later this read side becomes OBS render callback input.
    while (true) {
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
                std::cout << "[HID] connected=" << (state.connected ? 1 : 0)
                          << " b1=" << (state.button01 ? 1 : 0)
                          << " b2=" << (state.button02 ? 1 : 0)
                          << " b3=" << (state.button03 ? 1 : 0)
                          << " b4=" << (state.button04 ? 1 : 0)
                          << " b5=" << (state.button05 ? 1 : 0)
                          << " b6=" << (state.button06 ? 1 : 0)
                          << " b7=" << (state.button07 ? 1 : 0)
                          << " dir=" << state.xDirection
                          << "\n";
                last = state;
                hasLast = true;
            }
        }

        Sleep(5);
    }
}
