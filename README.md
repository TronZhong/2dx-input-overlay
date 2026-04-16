# 注意

这个项目主要是由ai写成的，利用了spice2x的逻辑，我改动了一点适配了我的鸽台的连接。

# Single HID Controller Demo (Standalone)

This folder is intentionally outside the spice2x source tree logic.
It provides a minimal, reusable template for detecting and parsing one HID controller.

## What is included

- my_hid_adapter.h: data structures and adapter interface
- my_hid_adapter.cpp: minimal HID parsing logic for one device
- main.cpp: standalone WM_INPUT test program for one HID controller
- CMakeLists.txt: build script for the standalone test program
- DEVICE_INFO_TEMPLATE.md: checklist for collecting your device info
- SPICE2X_INTEGRATION_NOTES.md: where to plug this into spice2x later

## Scope

This demo focuses on one HID device only:

- Match by VID/PID
- Read two buttons (Start/Service)
- Read two axes (X/Y)
- Keep logic simple and easy to inspect
- Print all detected HID devices at startup (VID/PID/UsagePage/Usage/Name)
- Throttle live state logs to reduce console spam

## Not included

- Full generic HID framework
- Full project build files
- Automatic config UI binding

## Typical workflow

1. Fill DEVICE_INFO_TEMPLATE.md with your real controller values.
2. Edit constants in my_hid_adapter.h / my_hid_adapter.cpp.
3. Validate parsed values in logs.
4. If needed, integrate into spice2x using SPICE2X_INTEGRATION_NOTES.md.

## Build and run (Windows)

Using CMake + MSVC:

```powershell
cd d:\Fork\spice2x\hid-single-controller-demo
cmake -S . -B build
cmake --build build --config Release
.\build\Release\single_hid_monitor.exe
```

If your generator is single-config (for example Ninja), run:

```powershell
cd d:\Fork\spice2x\hid-single-controller-demo
cmake -S . -B build -G Ninja
cmake --build build
.\build\single_hid_monitor.exe
```

Before running, open main.cpp and set your real VID/PID in main().

## Runtime output

On startup, the program prints all HID entries detected by Raw Input.
Use this list to find your target device VID/PID and usage values quickly.

During input, logs are throttled while still printing immediately for button changes
or larger axis movement.

## Code walkthrough (current project)

If you want to understand the code quickly, read in this order:

1. main.cpp
2. hid_input_backend.h / hid_input_backend.cpp
3. my_hid_adapter.h / my_hid_adapter.cpp

Runtime flow:

1. `main.cpp` sets VID/PID and starts `HidInputBackend`.
2. `HidInputBackend` starts a worker thread with a hidden Win32 window.
3. Worker registers Raw Input (`RIDEV_INPUTSINK | RIDEV_DEVNOTIFY`) for joystick/gamepad usages.
4. HID reports arrive as `WM_INPUT`; hot-plug arrives as `WM_INPUT_DEVICE_CHANGE`.
5. Backend filters devices by VID/PID, caches preparsed data, then calls `MyHidAdapter::updateFromReport(...)`.
6. Adapter decodes button usages + axis value and derives direction (`xDirection`).
7. Backend publishes a flattened `HidOverlayState` snapshot for consumers.
8. `main.cpp` currently acts as a demo consumer (polls snapshot and prints on change).

Why this structure helps OBS integration:

- Input acquisition and render are already separated.
- Backend can be started/stopped from OBS source lifecycle.
- Render side only reads `HidOverlayState` and draws; no HID API calls in render callback.

## Notes

- Usage page / usage values vary by device.
- Link collection can be required for some controllers.
- Vendor-specific HID pages (0xFFxx) may need special handling.
