#include "button_parser.h"

#include "platform/hid/hid_parser.h"
#include <vector>

namespace button_parser {

void parseButtons(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState) {

    // Use first button mapping's usagePage/linkCollection for the usage list query
    // (All buttons typically share the same usage page and link collection)
    if (config.buttons.empty()) {
        outState.dynamicButtons.clear();
        return;
    }

    const auto& firstBtn = config.buttons[0];
    std::vector<USAGE> activeUsages;
    hid_parser::getActiveButtonUsages(
        preparsed,
        firstBtn.usagePage,
        static_cast<USHORT>(firstBtn.linkCollection),
        rawData,
        static_cast<ULONG>(rawSize),
        activeUsages);

    outState.dynamicButtons.assign(config.buttons.size(), false);

    // Resolve each mapping against the full active-usage list
    for (size_t i = 0; i < config.buttons.size(); ++i) {
        const auto& btn = config.buttons[i];
        bool pressed = false;
        for (USAGE u : activeUsages) {
            if (u == btn.usage) {
                pressed = true;
                break;
            }
        }
        outState.dynamicButtons[i] = pressed;
    }
}

} // namespace button_parser