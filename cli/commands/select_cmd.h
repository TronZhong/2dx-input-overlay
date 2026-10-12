#pragma once

#include "command.h"

class SelectCommand : public Command {
public:
    bool execute(CliContext& ctx, const std::string& args) override;
    std::string name() const override { return "select"; }
    std::string usage() const override { return "s N - Select device by index"; }
};