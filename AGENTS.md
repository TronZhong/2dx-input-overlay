# AGENTS.md

本仓库是一个 Windows 专属的 OBS 插件 + 后端工程，用于实时显示 GAMO2 PHOENIXWAN+（LMT）HID 控制器输入。语言：后端 C++17，OBS Source 层 C。构建：CMake。

## 两个独立构建（不要混淆）

- 根目录 `CMakeLists.txt` -> 后端调试监控程序 `single_hid_monitor`，产物 `build/Release/single_hid_monitor.exe`。用根目录 `run_build.cmd` 构建。
- `obs-plugintemplate/` -> 交付用 OBS 插件 `2dx-input-overlay.dll`，安装到 `obs-plugintemplate/release/2dx-input-overlay/`。用 `obs-plugintemplate/run_build.cmd` 构建。
- 根目录构建**不会**构建插件。各自的 `run_build.cmd` 只针对自己目录。

## 关键：后端 ↔ OBS 前端必须同步

改了后端就必须同步改 OBS 侧。`HidOverlayState`（结构体在 `hid_input_backend.h`）是唯一读取路径：渲染线程不直接调用 HID/Raw Input API，只消费快照。
- 新增字段必须同步更新 3 处：
  1. `hid_input_backend.h`（结构体）
  2. `obs-plugintemplate/src/hid-backend-bridge.cpp`（+`.h`）（映射）
  3. `obs-plugintemplate/src/input-overlay-source.c`（渲染）
- 字段映射见 `HID_OVERLAY_STATE_SEMANTICS.md`，改动时必须同步。

## 设备参数存在多处（需保持一致）

`vid/pid`、按键 usage、轴 usage、`x_logical_min/max`、`x_idle_timeout_ms`：
- `my_hid_adapter.h`：`MyHidConfig` 默认字段（监控程序 + 桥接统一生效）。
- `obs-plugintemplate/src/input-overlay-source.c`：`input_overlay_defaults()`（新建 Source 默认值）与 `input_overlay_properties()`（面板可改项）。
- `main.cpp`：仅调试监控程序，不影响 OBS Source。

要想“一次改动全局生效”，至少同时更新 `my_hid_adapter.h` 与 `input-overlay-source.c`。

## 构建说明 / 坑

- OBS 预设自动回退：`windows-x64`(VS2026) -> `windows-vs2022-x64`。
- `obs-plugintemplate` 是本仓库的普通目录，**不是** git submodule。
- `scripts/fetch-deps.ps1`（依赖预取）若被中断，先清理 `.deps/.fetch-deps.lock` 与 `.deps/*.part` 再重跑。
- 依赖校验通过的标准：脚本输出 3 行 `PASS`，且与 `buildspec.json` 匹配。

## 相关文档是权威来源（改动相关代码前先读）

- `HID_OVERLAY_STATE_SEMANTICS.md`：快照字段语义 / 桥接映射。
- `OBS_MINIMAL_LOAD_STEPS.md`：编译 -> 安装 -> OBS 加载的完整流程。
- `MINIMAL_REGRESSION_CHECKLIST.md`：三段回归验证步骤。
- `DO_TODO_LIST.md`：AI 交接记录，含事项格式规范与验收标准。
