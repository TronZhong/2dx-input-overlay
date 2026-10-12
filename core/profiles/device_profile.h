#pragma once

#include "core/types/hid_types.h"
#include <string>
#include <optional>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct DeviceProfile {
    uint16_t vid = 0;
    uint16_t pid = 0;
    std::string name;
    MyHidConfig config;
};

class DeviceProfileManager {
public:
    explicit DeviceProfileManager(const fs::path& profilesDir = "profiles");

    std::optional<DeviceProfile> loadProfile(uint16_t vid, uint16_t pid) const;
    bool saveProfile(const MyHidConfig& config, const std::string& name = "");
    std::vector<DeviceProfile> listProfiles() const;
    bool deleteProfile(uint16_t vid, uint16_t pid);

    const fs::path& getProfilesDir() const { return profilesDir_; }

private:
    fs::path profilesDir_;

    fs::path profilePath(uint16_t vid, uint16_t pid) const;
    void ensureDirExists() const;
};