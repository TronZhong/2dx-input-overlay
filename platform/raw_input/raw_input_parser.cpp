#include "raw_input_parser.h"

#include <windows.h>
#include <hidsdi.h>
#include <string>

namespace {

std::string wideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return {};
    std::string result(static_cast<size_t>(sizeNeeded), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}

} // namespace

bool RawInputParser::parseRawInput(LPARAM lParam, RawInputData& out) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER))
        == static_cast<UINT>(-1) || size == 0) {
        return false;
    }

    std::vector<uint8_t> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return false;
    }

    auto* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
    if (raw->header.dwType != RIM_TYPEHID) {
        return false;
    }

    out.deviceHandle = raw->header.hDevice;
    out.rawData = std::vector<uint8_t>(raw->data.hid.bRawData, raw->data.hid.bRawData + raw->data.hid.dwSizeHid);
    out.dataSize = raw->data.hid.dwSizeHid;
    return true;
}

bool RawInputParser::getPreparsedData(HANDLE handle, std::vector<uint8_t>& out) {
    UINT size = 0;
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, nullptr, &size) == static_cast<UINT>(-1) || size == 0) {
        return false;
    }

    out.assign(size, 0);
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, out.data(), &size) == static_cast<UINT>(-1)) {
        return false;
    }
    return true;
}

std::string RawInputParser::getDeviceName(HANDLE handle) {
    UINT chars = 0;
    if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, nullptr, &chars) == static_cast<UINT>(-1) || chars == 0) {
        return {};
    }

    std::wstring name(chars, L'\0');
    if (GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, &name[0], &chars) == static_cast<UINT>(-1)) {
        return {};
    }
    if (!name.empty() && name.back() == L'\0') name.pop_back();
    return wideToUtf8(name);
}

bool RawInputParser::getDeviceInfo(HANDLE handle, RID_DEVICE_INFO& out) {
    out.cbSize = sizeof(out);
    UINT size = out.cbSize;
    return GetRawInputDeviceInfo(handle, RIDI_DEVICEINFO, &out, &size) != static_cast<UINT>(-1);
}