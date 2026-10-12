#include "config_mapper.h"

#include <string>

namespace config_mapper {

bool mapCapabilitiesToConfig(const HidCapabilities& caps, MyHidConfig& config) {
    if (!caps.valid) return false;

    config.buttons.clear();
    config.axes.clear();

    // Buttons
    for (size_t i = 0; i < caps.buttons.size(); ++i) {
        const auto& b = caps.buttons[i];
        ButtonMapping m;
        m.usagePage = 0x09;  // Button page
        m.usage = b.usage;
        m.linkCollection = b.linkCollection;
        m.name = b.name.empty() ? ("Btn " + std::to_string(b.usage)) : b.name;
        m.index = i;
        config.buttons.push_back(m);
    }

    // Axes (only range values)
    size_t axisIndex = 0;
    for (size_t i = 0; i < caps.values.size(); ++i) {
        const auto& v = caps.values[i];
        if (!v.isRange) continue;  // Only range values are axes

        AxisMapping m;
        m.usagePage = v.usagePage;
        m.usage = v.usage;
        m.linkCollection = v.linkCollection;
        m.logicalMin = v.logicalMin;
        m.logicalMax = v.logicalMax;
        m.name = v.name.empty() ? ("Axis " + std::to_string(v.usage)) : v.name;
        m.index = axisIndex;
        config.axes.push_back(m);
        ++axisIndex;
    }

    return !config.buttons.empty() || !config.axes.empty();
}

} // namespace config_mapper