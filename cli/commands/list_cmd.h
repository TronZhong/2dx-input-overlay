#pragma once

#include "command.h"

class ListCommand : public Command {
public:
    bool execute(CliContext& ctx, const std::string& args) override;
    std::string name() const override { return "list"; }
    std::string usage() const override { return "l - List all HID devices"; }
};