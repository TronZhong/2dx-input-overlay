#pragma once

#include "command.h"

class QuitCommand : public Command {
public:
    bool execute(CliContext& ctx, const std::string& args) override;
    std::string name() const override { return "quit"; }
    std::string usage() const override { return "q - Quit program"; }
};