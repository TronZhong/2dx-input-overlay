#pragma once

#include "my_hid_adapter.h"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

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

    // Load profile for specific VID/PID
    std::optional<DeviceProfile> loadProfile(uint16_t vid, uint16_t pid) const;

    // Save profile for current config
    bool saveProfile(const MyHidConfig& config, const std::string& name = "");

    // List all available profiles
    std::vector<DeviceProfile> listProfiles() const;

    // Delete profile
    bool deleteProfile(uint16_t vid, uint16_t pid);

    // Get profiles directory
    const fs::path& getProfilesDir() const { return profilesDir_; }

private:
    fs::path profilesDir_;

    fs::path profilePath(uint16_t vid, uint16_t pid) const;
    void ensureDirExists() const;
};