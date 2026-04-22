# 2DX Input Overlay - OBS 插件内部说明

本文档仅用于内部推进，不面向最终用户。

## 当前目标

- 只做一个实时显示控制器输入的 OBS Source 插件（Windows）。
- 通过 Raw Input + HID 解析稳定采集单设备输入。
- 对渲染侧只暴露快照状态（7 按钮 + 1 个 X 轴方向）。
- 保持主工程监控程序作为调试工具，不作为最终交付物。

## 技术栈确认

- 语言：C++17（输入后端与适配器）、C（OBS Source 层）。
- 构建系统：CMake。
- 平台：Windows（Win32 消息循环、Raw Input、HID API）。
- 系统 API：`GetRawInputData`/`GetRawInputDeviceInfo`、`hidsdi`（链接 `hid.lib`）。
- OBS 集成：`libobs` 插件模板（`obs-plugintemplate` 目录，已并入主仓库，非子模块）。
- 并发模型：后端采集线程 + 渲染线程读取快照（桥接层隔离）。

## 代码落点

- `main.cpp`：独立监控程序入口（调试用）。
- `hid_input_backend.h/.cpp`：输入线程、设备管理、快照发布。
- `my_hid_adapter.h/.cpp`：HID report 解析与方向判定。
- `obs-plugintemplate/src/input-overlay-source.c`：OBS Source 渲染与属性面板。
- `obs-plugintemplate/src/hid-backend-bridge.cpp`：OBS 与后端桥接。

## 构建入口

主工程调试构建：

```powershell
cd d:\Fork\2dx-input-overlay
run_build.cmd
```

等价手动命令：

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\single_hid_monitor.exe
```

OBS 插件构建：在具备 Visual Studio 2022 + Windows SDK 的环境下，按 `obs-plugintemplate` 的 CMake 流程执行。

说明：`obs-plugintemplate` 当前作为主仓库普通目录维护，不使用 Git submodule。

## 文档边界

- `README.md`：项目目标、技术栈与边界（本文件）。
- `DO_TODO_LIST.md`：优先级事项与交接记录。
- `DEVICE_INFO_TEMPLATE.md`：设备参数采集模板。
- `SPICE2X_INTEGRATION_NOTES.md`：历史预研文档，当前目标下冻结维护。

## 当前非目标

- spice2x 接入落地。
- 通用多设备映射。
- 完整用户配置 UI。
- 跨平台支持（当前仅 Windows）。

## 下阶段动作

1. 回填真实设备参数并校准 `my_hid_adapter`。
2. 在目标环境完成 OBS 插件目录最小构建验证。
3. 固化“编译 -> 安装 -> OBS 加载 Source -> 输入验证”的最小运行流程。
