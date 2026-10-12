#pragma once

#include "command.h"

class WriteCommand : public Command {
public:
    bool execute(CliContext& ctx, const std::string& args) override;
    std::string name() const override { return "write"; }
    std::string usage() const override { return "w [name] - Save current config as profile"; }
};