#include "quit_cmd.h"
#include "cli/commands/command.h"
#include <iostream>

bool QuitCommand::execute(CliContext& ctx, const std::string& args) {
    (void)args;
    std::cout << "Quitting...\n";
    ctx.running = false;
    return true;
}