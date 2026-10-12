#include "profile_serialization.h"

#include <algorithm>

namespace profile_serialization {

using json = nlohmann::json;

json buttonMappingToJson(const ButtonMapping& btn) {
    json j;
    j["usagePage"] = btn.usagePage;
    j["usage"] = btn.usage;
    j["linkCollection"] = btn.linkCollection;
    j["name"] = btn.name;
    j["index"] = btn.index;
    return j;
}

json axisMappingToJson(const AxisMapping& axis) {
    json j;
    j["usagePage"] = axis.usagePage;
    j["usage"] = axis.usage;
    j["linkCollection"] = axis.linkCollection;
    j["logicalMin"] = axis.logicalMin;
    j["logicalMax"] = axis.logicalMax;
    j["name"] = axis.name;
    j["index"] = axis.index;
    return j;
}

ButtonMapping buttonMappingFromJson(const json& j) {
    ButtonMapping btn;
    btn.usagePage = j.value("usagePage", (USAGE)0x09);
    btn.usage = j.value("usage", (USAGE)0);
    btn.linkCollection = j.value("linkCollection", (ULONG)0);
    btn.name = j.value("name", std::string());
    btn.index = j.value("index", (size_t)0);
    return btn;
}

AxisMapping axisMappingFromJson(const json& j) {
    AxisMapping axis;
    axis.usagePage = j.value("usagePage", (USAGE)0x01);
    axis.usage = j.value("usage", (USAGE)0x30);
    axis.linkCollection = j.value("linkCollection", (ULONG)0);
    axis.logicalMin = j.value("logicalMin", (LONG)0);
    axis.logicalMax = j.value("logicalMax", (LONG)255);
    axis.name = j.value("name", std::string());
    axis.index = j.value("index", (size_t)0);
    return axis;
}

// Legacy field name constants
namespace legacy {
    constexpr const char* buttonUsagePage = "buttonUsagePage";
    constexpr const char* buttonUsages[7] = {
        "button_01Usage", "button_02Usage", "button_03Usage",
        "button_04Usage", "button_05Usage", "button_06Usage", "button_07Usage"
    };
    constexpr const char* buttonLinkCollection = "buttonLinkCollection";
    constexpr const char* axisUsagePage = "axisUsagePage";
    constexpr const char* xUsage = "xUsage";
    constexpr const char* axisLinkCollection = "axisLinkCollection";
    constexpr const char* enableYAxis = "enableYAxis";
    constexpr const char* xLogicalMin = "xLogicalMin";
    constexpr const char* xLogicalMax = "xLogicalMax";
    constexpr const char* xIdleTimeoutMs = "xIdleTimeoutMs";
}

json configToJson(const MyHidConfig& cfg) {
    json j;
    j["vid"] = cfg.vid;
    j["pid"] = cfg.pid;

    // Dynamic mappings
    json buttonsJson = json::array();
    for (const auto& btn : cfg.buttons) {
        buttonsJson.push_back(buttonMappingToJson(btn));
    }
    j["buttons"] = buttonsJson;

    json axesJson = json::array();
    for (const auto& axis : cfg.axes) {
        axesJson.push_back(axisMappingToJson(axis));
    }
    j["axes"] = axesJson;

    return j;
}

MyHidConfig configFromJson(const json& j) {
    MyHidConfig cfg;
    cfg.vid = static_cast<uint16_t>(j.value("vid", 0x034C));
    cfg.pid = static_cast<uint16_t>(j.value("pid", 0x0368));

    // 1) Prefer dynamic mappings (new format)
    bool hasDynamicButtons = j.contains("buttons") && j["buttons"].is_array();
    bool hasDynamicAxes = j.contains("axes") && j["axes"].is_array();

    if (hasDynamicButtons) {
        for (const auto& btnJson : j["buttons"]) {
            cfg.buttons.push_back(buttonMappingFromJson(btnJson));
        }
    }
    if (hasDynamicAxes) {
        for (const auto& axisJson : j["axes"]) {
            cfg.axes.push_back(axisMappingFromJson(axisJson));
        }
    }

    // 2) Legacy compatibility: if no dynamic mappings, rebuild from legacy fields
    if (!hasDynamicButtons && j.contains(legacy::buttonUsagePage)) {
        USAGE page = static_cast<USAGE>(j.value(legacy::buttonUsagePage, 0x09));
        ULONG link = static_cast<ULONG>(j.value(legacy::buttonLinkCollection, 0));
        for (int i = 0; i < 7; ++i) {
            USAGE usage = static_cast<USAGE>(j.value(legacy::buttonUsages[i], (USAGE)(i + 1)));
            if (usage != 0) {
                ButtonMapping btn;
                btn.usagePage = page;
                btn.usage = usage;
                btn.linkCollection = link;
                btn.name = "Btn " + std::to_string(usage);
                btn.index = static_cast<size_t>(i);
                cfg.buttons.push_back(btn);
            }
        }
    }

    if (!hasDynamicAxes && j.contains(legacy::axisUsagePage)) {
        USAGE page = static_cast<USAGE>(j.value(legacy::axisUsagePage, 0x01));
        ULONG link = static_cast<ULONG>(j.value(legacy::axisLinkCollection, 0));
        LONG lmin = static_cast<LONG>(j.value(legacy::xLogicalMin, 0));
        LONG lmax = static_cast<LONG>(j.value(legacy::xLogicalMax, 255));
        USAGE usage = static_cast<USAGE>(j.value(legacy::xUsage, 0x30));

        AxisMapping axis;
        axis.usagePage = page;
        axis.usage = usage;
        axis.linkCollection = link;
        axis.logicalMin = lmin;
        axis.logicalMax = lmax;
        axis.name = "X";
        axis.index = 0;
        cfg.axes.push_back(axis);

        // Optional Y axis (legacy enableYAxis)
        if (j.value(legacy::enableYAxis, false)) {
            AxisMapping yAxis = axis;
            yAxis.usage = 0x31; // Y
            yAxis.name = "Y";
            yAxis.index = 1;
            cfg.axes.push_back(yAxis);
        }
    }

    return cfg;
}

} // namespace profile_serialization