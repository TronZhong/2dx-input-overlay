#pragma once

#include "command.h"

class PrintCommand : public Command {
public:
    bool execute(CliContext& ctx, const std::string& args) override;
    std::string name() const override { return "print"; }
    std::string usage() const override { return "p - Print current config"; }
};