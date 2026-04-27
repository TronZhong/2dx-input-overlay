# OBS 最小运行步骤（内部）

最后更新时间：2026-04-24

目标：让接手者从“编译”走到“OBS 内成功加载 Source”的最小路径。

## 0. 前置环境

- Windows 10/11 x64
- Visual Studio 2026（含 C++ 桌面工具链）
- Windows SDK（满足 `obs-plugintemplate` 依赖）
- CMake（建议 >= 3.30）
- 已安装 OBS Studio（与模板依赖版本兼容）

## 1. 预取依赖

在 PowerShell 执行：

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1
```

成功判定：
- 输出包含三行 `PASS`：
  - `windows-deps-2025-07-11-x64.zip`
  - `windows-deps-qt6-2025-07-11-x64.zip`
  - `31.1.1.zip`

失败处理：
- 若提示 lock 文件冲突，确认不存在并行的 `fetch-deps.ps1`。
- 若下载失败，按脚本输出的 `Failed files` 定位重试。

## 2. 构建插件

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
cmd /c run_build.cmd
```

成功判定：
- 末尾出现 `[OK] OBS plugin build completed via preset windows-x64.`

## 3. 在 OBS 中加载

1. 将构建产物复制到 OBS 插件目录（按本机安装目录结构放置）。
2. 启动 OBS。
3. 在场景中新增 Source：`2DX Input Overlay`。
4. 打开 Source 属性，填写：
   - `device_vid`
   - `device_pid`
   - `button_usage_page`、`button_01_usage` ... `button_07_usage`
   - `axis_usage_page`、`x_usage`、`x_logical_min`、`x_logical_max`

成功判定：
- Source 能正常创建。
- 按键按下时七格状态实时变化。
- 旋钮转动时方向与标记实时变化。
- 设备断开后回落到断连状态，重连后恢复。

## 4. 常见故障

- 依赖阶段失败：先清理并发下载，再重跑 `scripts/fetch-deps.ps1`。
- 生成器不匹配：确认 `CMakePresets.json` 中 `windows-x64` 所需 VS 实例路径可用。
- Source 不显示：先检查插件文件是否放在正确架构目录，再检查 OBS 启动日志中的模块加载错误。
