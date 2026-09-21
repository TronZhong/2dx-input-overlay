# AGENTS.md

本仓库是一个 Windows 专属的独立控制台程序，用于枚举已连接 HID 设备、选择单个设备、自动检测其全部按键与轴值输入信号并实时打印。语言 C++17，构建 CMake。OBS 插件前端方案已删除。

## 构建 / 入口

- 单一目标：`single_hid_monitor`，产物 `build/Release/single_hid_monitor.exe`。
- 入口为 `main.cpp`（交互式命令 `l`/`s N`/`p`/`w`/`q`）。
- 用根目录 `run_build.cmd` 构建（`cmake -S . -B build` + `cmake --build build --config Release`）。
- 无 OBS 子项目；`obs-plugintemplate/` 已删除。

## 关键：自动检测（新核心能力）

选择设备（`s N`）后，后端读取设备 HID 描述符 `PHIDP_PREPARSED_DATA`，用：
- `HidP_GetButtonCaps` 枚举全部按钮 usage
- `HidP_GetValueCaps` 枚举全部轴值/标量 usage（含 `logicalMin/Max`）
自动生成 `MyHidConfig.buttons` / `MyHidConfig.axes` 动态映射，直接用于解析。

涉及文件：
- `HidCapabilities.h/.cpp`：能力枚举（`CapabilityInspector::enumerate`）。
- `my_hid_adapter.h/.cpp`：`autoDetect` 生成动态映射；`updateFromReport` 动态优先。
- `hid_input_backend.h/.cpp`：`autoConfigure`（绑定+枚举+应用）、`setTargetVidPid`、`getConfig`。
- `main.cpp`：交互命令接线。

## 动态映射 vs 旧固定 7+1

- `MyHidConfig` 含动态 `vector<ButtonMapping> buttons` 和 `vector<AxisMapping> axes`，以及旧固定 7+1 字段（兼容保留）。
- `updateFromReport`：`buttons`/`axes` 非空时按动态解析；否则回退旧字段。

## 设备 Profile

- `DeviceProfile.h/.cpp`：`profiles/<vid>_<pid>.json` 存完整映射（含动态 buttons/axes）。
- 用 nlohmann/json（vendored 于 `third_party/`，无外部依赖）。
- `s N` 时若存在对应 profile 优先加载，否则自动检测。

## 构建 / 依赖注意

- nlohmann/json 在 `third_party/`，CMake include 路径指向它，不依赖任何 `.deps` 目录。
- `HIDP_*_CAPS` 结构为 union 形式（`Range`/`NotRange`），计数参数为 `USHORT`；改代码时按 SDK 实际结构。

## 相关文档

- `README.md`：项目目标、技术栈、交互命令。
- `HID_OVERLAY_STATE_SEMANTICS.md`：`HidOverlayState` 字段语义。
- `DEVICE_INFO_TEMPLATE.md`：设备参数采集模板。
- `DO_TODO_LIST.md`：待办与交接记录。
