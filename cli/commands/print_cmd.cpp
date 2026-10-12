#include "print_cmd.h"
#include "cli/commands/command.h"
#include "cli/formatters/config_formatter.h"
#include <iostream>

bool PrintCommand::execute(CliContext& ctx, const std::string& args) {
    (void)args;
    config_formatter::printConfig(ctx.config);
    return true;
}