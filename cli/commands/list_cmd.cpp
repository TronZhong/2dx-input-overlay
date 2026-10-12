#include "list_cmd.h"
#include "cli/commands/command.h"
#include "platform/enumerator/device_enumerator.h"
#include "cli/formatters/device_formatter.h"

bool ListCommand::execute(CliContext& ctx, const std::string& args) {
    (void)args;
    ctx.devices = DeviceEnumerator::enumerateAll();
    device_formatter::printDeviceList(ctx.devices);
    return true;
}