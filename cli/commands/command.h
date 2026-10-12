#pragma once

#include "core/types/hid_types.h"
#include "core/profiles/device_profile.h"
#include "backend/hid_input_backend.h"
#include <vector>
#include <string>

struct CliContext;

class Command {
public:
    virtual ~Command() = default;
    virtual bool execute(CliContext& ctx, const std::string& args) = 0;
    virtual std::string name() const = 0;
    virtual std::string usage() const = 0;
};

struct CliContext {
    HidInputBackend& backend;
    DeviceProfileManager& profileMgr;
    std::vector<DeviceInfo> devices;
    MyHidConfig config;
    bool running = true;
};