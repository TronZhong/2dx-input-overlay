# AGENTS.md

本仓库是一个 Windows 专属的独立控制台程序，用于枚举已连接 HID 设备、选择单个设备、自动检测其全部按键与轴值输入信号并实时打印。语言 C++17，构建 CMake。OBS 插件前端方案已删除。

## 构建 / 入口

- 单一目标：`single_hid_monitor`，产物 `build/Release/single_hid_monitor.exe`。
- 入口为 `main.cpp`（交互式命令 `l`/`s N`/`p`/`w`/`q`）。
- 用根目录 `run_build.cmd` 构建（`cmake -S . -B build` + `cmake --build build --config Release`）。
- 无 OBS 子项目；`obs-plugintemplate/` 已删除。

## 自动检测

选择设备（`s N`）后，后端读取设备 HID 描述符 `PHIDP_PREPARSED_DATA`，用：
- `HidP_GetButtonCaps` 枚举全部按钮 usage
- `HidP_GetValueCaps` 枚举全部轴值/标量 usage（含 `logicalMin/Max`）
自动生成 `MyHidConfig.buttons` / `MyHidConfig.axes` 动态映射，直接用于解析。

涉及文件：
- `core/capabilities/hid_capabilities.h/.cpp`：能力枚举（`CapabilityInspector::enumerate`）。
- `adapter/my_hid_adapter.h/.cpp`：`autoDetect` 生成动态映射；`updateFromReport` 解析报告。
- `backend/hid_input_backend.h/.cpp`：`autoConfigure`（绑定+枚举+应用）、`setTargetVidPid`、`getConfig`。
- `main.cpp` 与 `cli/commands/`：交互命令接线。

## 动态映射与旧 Profile 兼容

- `MyHidConfig` 以动态 `vector<ButtonMapping> buttons` 和 `vector<AxisMapping> axes` 描述映射。
- 旧格式 Profile 由 `core/profiles/profile_serialization.cpp` 迁移为动态映射；保存时写入新格式。
- Report 解析通过 `adapter/` 中的按钮、轴解析器处理动态映射。

## 设备 Profile

- `core/profiles/device_profile.h/.cpp`：`profiles/<vid>_<pid>.json` 存完整映射（含动态 buttons/axes）。
- 用 nlohmann/json（vendored 于 `third_party/`，无外部依赖）。
- `s N` 时若存在对应 profile 优先加载，否则自动检测。

## 构建 / 依赖注意

- nlohmann/json 在 `third_party/`，CMake include 路径指向它，不依赖任何 `.deps` 目录。
- `HIDP_*_CAPS` 结构为 union 形式（`Range`/`NotRange`），计数参数为 `USHORT`；改代码时按 SDK 实际结构。

## 文档入口

- `README.md`：项目简介、快速开始与文档入口。
- `docs/index.md`：按使用、开发、架构和模块细节组织的文档索引。
- `docs/architecture.md`、`docs/data_flow.md`、`docs/build.md`：项目级设计与开发指南。
- 各模块目录中的 `.md`：对应模块的实现说明。
- `DO_TODO_LIST.md`：项目内部待办与交接记录。
