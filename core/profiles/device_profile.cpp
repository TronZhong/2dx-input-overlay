#include "device_profile.h"
#include "profile_serialization.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

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
        profile.config = profile_serialization::configFromJson(j);
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
        json j = profile_serialization::configToJson(config);
        j["name"] = name;

        std::ofstream file(path);
        file << j.dump(2);
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
                    profile.config = profile_serialization::configFromJson(j);
                    result.push_back(std::move(profile));
                }
            } catch (const std::exception&) {
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