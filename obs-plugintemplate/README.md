# 2DX Input Overlay OBS 子项目（内部）

最后更新时间：2026-04-29

本目录是主仓库中的 OBS 插件子项目，当前以 Windows 本地构建与联调为主。

## 最小构建命令

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
cmd /c run_build.cmd
```

## 当前构建策略

- `run_build.cmd` 会先检查 CMake 是否在 PATH 中。
- 优先使用 `windows-x64`（Visual Studio 18 2026）预设。
- 若主预设不可用，会自动回退到 `windows-vs2022-x64`（Visual Studio 17 2022）预设。
- 构建完成后执行 `cmake --install`，统一输出到单一目录 `release/`。
- 目录布局按 OBS 插件结构组织，例如：
	- `release/2dx-input-overlay/bin/64bit/2dx-input-overlay.dll`
	- `release/2dx-input-overlay/data/locale/en-US.ini`

## 前置依赖

- Windows 10/11 x64
- CMake（建议 >= 3.30）
- Visual Studio 2026 或 2022（包含 C++ 桌面开发工具链）

## 依赖预取

```powershell
Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1
```

若下载中断，先清理 `.deps/.fetch-deps.lock` 与 `.deps/*.part` 再重试。
