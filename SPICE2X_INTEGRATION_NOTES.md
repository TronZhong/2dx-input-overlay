# spice2x Integration Notes (Reference Only)

This file explains where to connect the standalone template into spice2x later.
No spice2x source files are modified in this demo folder.

## Integration points

1. HID scan stage
- File: src/spice2x/rawinput/rawinput.cpp
- Region: devices_scan_rawinput(...), HID branch
- Goal: detect your VID/PID and mark/log target device

2. HID input processing stage
- File: src/spice2x/rawinput/rawinput.cpp
- Region: input_wnd_proc(...), case WM_INPUT, HID branch
- Goal: pass preparsed data + raw report to MyHidAdapter::updateFromReport

3. Button mapping stage
- Files: src/spice2x/cfg/button.h and src/spice2x/cfg/api.cpp
- Goal: map parsed fields (start/service/x/y) to game button definitions

## Minimal pseudo-call in WM_INPUT HID branch

```cpp
// Pseudocode only
if (isMyTargetDevice(device)) {
    MyHidState state;
    adapter.updateFromReport(
        reinterpret_cast<PHIDP_PREPARSED_DATA>(device.hidInfo->preparsed_data.get()),
        reinterpret_cast<const uint8_t*>(data_hid.bRawData),
        data_hid.dwSizeHid,
        state);

    // Write state into your preferred internal storage.
    // Then your button-read path can consume it.
}
```

## Why this split is useful

- You can test HID parsing logic in isolation first.
- You avoid editing large sections of spice2x while still learning the flow.
- You can iterate device constants quickly (VID/PID/usages/link collection).
