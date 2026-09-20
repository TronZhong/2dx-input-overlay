#include "DeviceProfile.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace {

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

json configToJson(const MyHidConfig& cfg) {
    json j;
    j["vid"] = cfg.vid;
    j["pid"] = cfg.pid;
    j["buttonUsagePage"] = cfg.buttonUsagePage;
    j["button_01Usage"] = cfg.button_01Usage;
    j["button_02Usage"] = cfg.button_02Usage;
    j["button_03Usage"] = cfg.button_03Usage;
    j["button_04Usage"] = cfg.button_04Usage;
    j["button_05Usage"] = cfg.button_05Usage;
    j["button_06Usage"] = cfg.button_06Usage;
    j["button_07Usage"] = cfg.button_07Usage;
    j["buttonLinkCollection"] = cfg.buttonLinkCollection;
    j["axisUsagePage"] = cfg.axisUsagePage;
    j["xUsage"] = cfg.xUsage;
    j["axisLinkCollection"] = cfg.axisLinkCollection;
    j["enableYAxis"] = cfg.enableYAxis;
    j["xLogicalMin"] = cfg.xLogicalMin;
    j["xLogicalMax"] = cfg.xLogicalMax;
    j["xIdleTimeoutMs"] = cfg.xIdleTimeoutMs;

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
    cfg.buttonUsagePage = static_cast<USAGE>(j.value("buttonUsagePage", 0x09));
    cfg.button_01Usage = static_cast<USAGE>(j.value("button_01Usage", 0x01));
    cfg.button_02Usage = static_cast<USAGE>(j.value("button_02Usage", 0x02));
    cfg.button_03Usage = static_cast<USAGE>(j.value("button_03Usage", 0x03));
    cfg.button_04Usage = static_cast<USAGE>(j.value("button_04Usage", 0x04));
    cfg.button_05Usage = static_cast<USAGE>(j.value("button_05Usage", 0x05));
    cfg.button_06Usage = static_cast<USAGE>(j.value("button_06Usage", 0x06));
    cfg.button_07Usage = static_cast<USAGE>(j.value("button_07Usage", 0x07));
    cfg.buttonLinkCollection = static_cast<ULONG>(j.value("buttonLinkCollection", 0));
    cfg.axisUsagePage = static_cast<USAGE>(j.value("axisUsagePage", 0x01));
    cfg.xUsage = static_cast<USAGE>(j.value("xUsage", 0x30));
    cfg.axisLinkCollection = static_cast<ULONG>(j.value("axisLinkCollection", 0));
    cfg.enableYAxis = j.value("enableYAxis", false);
    cfg.xLogicalMin = static_cast<LONG>(j.value("xLogicalMin", 0));
    cfg.xLogicalMax = static_cast<LONG>(j.value("xLogicalMax", 255));
    cfg.xIdleTimeoutMs = static_cast<uint32_t>(j.value("xIdleTimeoutMs", 99));

    // Dynamic mappings
    if (j.contains("buttons") && j["buttons"].is_array()) {
        for (const auto& btnJson : j["buttons"]) {
            cfg.buttons.push_back(buttonMappingFromJson(btnJson));
        }
    }
    if (j.contains("axes") && j["axes"].is_array()) {
        for (const auto& axisJson : j["axes"]) {
            cfg.axes.push_back(axisMappingFromJson(axisJson));
        }
    }

    return cfg;
}

} // namespace

DeviceProfileManager::DeviceProfileManager(const fs::path& profilesDir)
    : profilesDir_(profilesDir) {
    ensureDirExists();
}

void DeviceProfileManager::ensureDirExists() const {
    std::error_code ec;
    fs::create_directories(profilesDir_, ec);
}

fs::path DeviceProfileManager::profilePath(uint16_t vid, uint16_t pid) const {
    char filename[32];
    sprintf_s(filename, "%04X_%04X.json", vid, pid);
    return profilesDir_ / filename;
}

std::optional<DeviceProfile> DeviceProfileManager::loadProfile(uint16_t vid, uint16_t pid) const {
    fs::path path = profilePath(vid, pid);
    if (!fs::exists(path)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(path);
        json j;
        file >> j;

        DeviceProfile profile;
        profile.vid = vid;
        profile.pid = pid;
        profile.name = j.value("name", "");
        profile.config = configFromJson(j);
        return profile;
    } catch (const std::exception& e) {
        std::cerr << "[Profile] Failed to load " << path << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

bool DeviceProfileManager::saveProfile(const MyHidConfig& config, const std::string& name) {
    ensureDirExists();

    fs::path path = profilePath(config.vid, config.pid);

    try {
        json j = configToJson(config);
        j["name"] = name;

        std::ofstream file(path);
        file << j.dump(2); // pretty print with 2-space indent
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Profile] Failed to save " << path << ": " << e.what() << "\n";
        return false;
    }
}

std::vector<DeviceProfile> DeviceProfileManager::listProfiles() const {
    std::vector<DeviceProfile> result;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(profilesDir_, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            try {
                std::ifstream file(entry.path());
                json j;
                file >> j;

                uint16_t vid = static_cast<uint16_t>(j.value("vid", 0));
                uint16_t pid = static_cast<uint16_t>(j.value("pid", 0));
                if (vid != 0 && pid != 0) {
                    DeviceProfile profile;
                    profile.vid = vid;
                    profile.pid = pid;
                    profile.name = j.value("name", "");
                    profile.config = configFromJson(j);
                    result.push_back(std::move(profile));
                }
            } catch (const std::exception&) {
                // Skip invalid files
            }
        }
    }

    return result;
}

bool DeviceProfileManager::deleteProfile(uint16_t vid, uint16_t pid) {
    fs::path path = profilePath(vid, pid);
    if (fs::exists(path)) {
        std::error_code ec;
        fs::remove(path, ec);
        return !ec;
    }
    return false;
}