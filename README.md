# 2DX Input Overlay

Windows 专属的 HID 输入监视控制台程序。它可以列出已连接的 HID 设备，自动检测所选设备的按钮与轴，并实时显示输入状态。

当前仓库聚焦于设备枚举、输入解析和诊断输出；OBS 插件前端已不在本项目范围内。

## 快速开始

### 环境要求

- Windows 10/11
- Visual Studio 2022 或 Visual Studio 2022 Build Tools（含 Windows SDK）
- CMake 3.20+

### 构建与运行

在仓库根目录运行：

```cmd
run_build.cmd
build\Release\single_hid_monitor.exe
```

程序启动后提供以下命令：

| 命令 | 说明 |
|------|------|
| `l` | 列出 HID 设备 |
| `s N` | 选择设备；无 profile 时自动检测映射，有 profile 时切换目标 VID/PID |
| `p` | 打印当前映射配置 |
| `w [name]` | 保存当前映射为 profile |
| `q` | 退出 |

Profile 默认保存到运行目录下的 `profiles/<VID>_<PID>.json`。

**当前限制：** `s N` 找到 profile 时 CLI 会显示 profile 映射，但 backend 目前只接收 VID/PID，尚未应用 profile 中的按钮/轴映射。启动时加载的默认 profile 也只更新 CLI 配置副本。

## 文档

从[文档索引](docs/index.md)开始，可按目标查看使用、构建、架构、数据流和模块说明。

## 项目结构

```text
adapter/       HID 报告解析与配置映射
backend/       设备管理、自动配置与状态发布
cli/           命令与终端输出
core/          数据类型、HID 能力和 profile
docs/          项目级设计与开发文档
platform/      Windows HID、Raw Input 和设备枚举封装
third_party/   随仓库提供的 nlohmann/json
main.cpp       程序入口
```

## 技术栈

- C++17、CMake
- Windows Raw Input 与 HID API
- nlohmann/json（vendored，无需单独安装）
