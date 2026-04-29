# OBS 最小运行步骤（内部）

最后更新时间：2026-04-27

目标：让接手者从“编译”走到“OBS 内成功加载 Source”的最小路径。

## 0. 前置环境

- Windows 10/11 x64
- Visual Studio 2026（含 C++ 桌面工具链，供 `windows-x64` preset 自动发现）
- Windows SDK（满足 `obs-plugintemplate` 依赖）
- CMake（建议 >= 3.30）
- 已安装 OBS Studio（与模板依赖版本兼容）

## 1. 预取依赖

在 PowerShell 执行：

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1
```

当前实现说明：
- 脚本会从 `buildspec.json` 派生三个目标文件名、URL 与 SHA256。
- `obs-deps` 两个压缩包会先通过 GitHub release API 解析 asset URL，再优先使用 `curl.exe` 下载；`31.1.1.zip` 仍走 `codeload.github.com`。
- 脚本会在开始和结束阶段检查残留 `curl` 进程，并使用 `.deps/.fetch-deps.lock` 防止并发执行。

成功判定：
- 输出包含三行 `PASS`：
  - `windows-deps-2025-07-11-x64.zip`
  - `windows-deps-qt6-2025-07-11-x64.zip`
  - `31.1.1.zip`

失败处理：
- 若提示 lock 文件冲突，确认不存在并行的 `fetch-deps.ps1`。
- 若下载失败，按脚本输出的 `Failed files` 定位重试。
- 若上一次下载被中断，先清理 `.deps/.fetch-deps.lock` 与 `.deps/*.part`，再重跑脚本，避免命中旧的半包和锁文件。
- 若只做链路验证，`-MaxRetry 1` 可以快速暴露失败文件；正式预取时不要带这个参数。

## 2. 构建插件

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
cmd /c run_build.cmd
```

成功判定：
- 末尾出现 `[OK] OBS plugin build completed via preset windows-x64.`

## 3. 生成可分发包（推荐）

在 PowerShell 执行：

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
cmd /c run_release_windows.cmd
```

说明：
- 脚本会执行 `configure -> build -> cmake --install -> zip`。
- 产物为 `release/<name>-<version>-windows-x64.zip`。
- 解压后保持目录结构，覆盖到 OBS 安装目录即可。

## 4. 本机直装到 OBS 目录（调试）

如果只做本机联调，可以直接安装到 OBS 的插件目录，不需要手工拷文件。

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
cmake --install build_x64 --config RelWithDebInfo --prefix "C:/ProgramData/obs-studio/plugins"
```

## 5. 在 OBS 中加载

1. 启动 OBS。
2. 在场景中新增 Source：`2DX Input Overlay`。
3. 打开 Source 属性，填写：
   - `device_vid`
   - `device_pid`
   - `button_usage_page`、`button_01_usage` ... `button_07_usage`
   - `axis_usage_page`、`x_usage`、`x_logical_min`、`x_logical_max`

成功判定：
- Source 能正常创建。
- 按键按下时七格状态实时变化。
- 旋钮转动时方向与标记实时变化。
- 设备断开后回落到断连状态，重连后恢复。

## 6. 常见故障

- 依赖阶段失败：先清理并发下载，再重跑 `scripts/fetch-deps.ps1`。
- 依赖阶段被中断：执行 `Remove-Item .deps/.fetch-deps.lock -Force -ErrorAction SilentlyContinue` 与 `Get-ChildItem .deps -Filter *.part | Remove-Item -Force -ErrorAction SilentlyContinue` 后再重跑。
- 生成器不匹配：确认本机已安装 Visual Studio 2026，并可被 CMake 自动发现；当前 preset 不再依赖固定 `CMAKE_GENERATOR_INSTANCE` 路径。
- Source 不显示：先检查插件文件是否放在正确架构目录，再检查 OBS 启动日志中的模块加载错误。
