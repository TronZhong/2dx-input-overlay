#include "hid_parser.h"

#include <windows.h>
#include <hidsdi.h>
#include <vector>

namespace hid_parser {

bool getActiveButtonUsages(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    const uint8_t* report,
    ULONG reportSize,
    std::vector<USAGE>& outUsages) {

    ULONG usageLength = 64;
    outUsages.assign(usageLength, 0);
    
    NTSTATUS status = HidP_GetUsages(
        HidP_Input,
        usagePage,
        linkCollection,
        outUsages.data(),
        &usageLength,
        preparsed,
        reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)),
        reportSize);

    if (status == HIDP_STATUS_SUCCESS) {
        outUsages.resize(usageLength);
        return true;
    }
    return false;
}

bool getUsageValue(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    const uint8_t* report,
    ULONG reportSize,
    LONG& outValue) {

    LONG raw = 0;
    NTSTATUS status = HidP_GetUsageValue(
        HidP_Input,
        usagePage,
        linkCollection,
        usage,
        reinterpret_cast<PULONG>(&raw),
        preparsed,
        reinterpret_cast<PCHAR>(const_cast<uint8_t*>(report)),
        reportSize);

    if (status == HIDP_STATUS_SUCCESS) {
        outValue = raw;
        return true;
    }
    return false;
}

} // namespace hid_parser