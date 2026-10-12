#include "write_cmd.h"
#include "cli/commands/command.h"
#include <iostream>
#include <cstdio>

bool WriteCommand::execute(CliContext& ctx, const std::string& args) {
    std::string name;
    if (!args.empty()) {
        name = args;
    }
    if (name.empty()) {
        char defaultName[64];
        sprintf_s(defaultName, "Device_%04X_%04X", ctx.config.vid, ctx.config.pid);
        name = defaultName;
    }
    if (ctx.profileMgr.saveProfile(ctx.config, name)) {
        std::cout << "Profile saved for VID=0x" << std::hex << ctx.config.vid << std::dec
                  << " PID=0x" << std::hex << ctx.config.pid << std::dec
                  << " as '" << name << "'\n";
    } else {
        std::cerr << "Failed to save profile.\n";
    }
    return true;
}