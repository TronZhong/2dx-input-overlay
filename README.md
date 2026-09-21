# 2DX Input Overlay - 独立 HID 输入诊断程序（内部）

简述：这是一个 Windows 专属的独立控制台程序，用于枚举已连接 HID 设备、选择单一设备、自动检测其全部按键与轴值输入信号并实时打印。当前方向已删除 OBS 插件前端方案，改为纯独立后端 + 未来独立窗口显示。

## 当前目标

- 枚举系统内所有 HID 设备，供用户选择单个设备。
- 选择设备后自动检测其全部按键与轴值（通过 `HidP_GetButtonCaps`/`HidP_GetValueCaps` 从 HID 描述符枚举），自动生成动态映射。
- 按动态映射解析输入，实时打印按键状态与轴值。
- Profile 系统（`profiles/<vid>_<pid>.json`）保存/恢复设备映射。
- 前端渲染（独立窗口 + 图片合成）为后续 P2 待办，当前未实施。

## 技术栈

- 语言：C++17。
- 构建系统：CMake（`run_build.cmd`）。
- 平台：Windows（Win32 消息循环、Raw Input、HID API `hidsdi`，链接 `hid.lib`）。
- JSON：nlohmann/json（vendor 在 `third_party/`）。

## 代码落点

- `main.cpp`：交互式入口，命令 `l`/`s N`/`p`/`w`/`q`。
- `hid_input_backend.h/.cpp`：输入线程、设备管理、快照发布、`setTargetVidPid`/`autoConfigure`。
- `my_hid_adapter.h/.cpp`：HID report 解析、`autoDetect` 动态映射。
- `HidCapabilities.h/.cpp`：用 `HidP_GetButtonCaps`/`HidP_GetValueCaps` 枚举设备全部按钮与轴值。
- `DeviceEnumerator.h/.cpp`：枚举系统 HID 设备列表。
- `DeviceProfile.h/.cpp`：JSON 配置读写（`profiles/`）。
- `third_party/nlohmann/`：vendored json。

## 关键设计

- **自动检测**：选择设备后，后端读取其 HID 描述符（`PHIDP_PREPARSED_DATA`），枚举全部按钮 usage 与轴值 usage（含 `logicalMin/Max`），自动生成 `MyHidConfig.buttons` / `MyHidConfig.axes`。
- **动态映射优先**：`updateFromReport` 在 `cfg.buttons`/`cfg.axes` 非空时按动态配置解析；否则回退到 legacy 固定 7+1 字段（为兼容保留）。
- **单设备选择**：`s N` 用 `autoConfigure` 绑定并自动检测；有对应 profile 时优先加载，否则自动检测。

## 构建 / 运行

```powershell
cd d:\Fork\2dx-input-overlay
run_build.cmd
.\build\Release\single_hid_monitor.exe
```

交互命令：
- `l`：列出当前 HID 设备。
- `s N`：选择设备 → 自动检测并打印全部按钮/轴值映射，开始解析输入。
- `p`：打印当前配置（含动态映射）。
- `w [name]`：把当前配置保存为 profile。
- `q`：退出。

## 文档边界

- `README.md`：项目目标、技术栈与边界（本文件）。
- `DO_TODO_LIST.md`：待办与交接记录。
- `DEVICE_INFO_TEMPLATE.md`：设备参数采集模板。
- `HID_OVERLAY_STATE_SEMANTICS.md`：`HidOverlayState` 字段语义。

## 当前非目标

- OBS 插件方案（已删除 `obs-plugintemplate/`）。
- spice2x 接入落地。
- 完整用户配置 UI。
- 跨平台支持（当前仅 Windows）。
