# DO-TODO List (AI Handoff)

最后更新时间：2026-09-20

## 现状摘要

仓库已从“OBS 插件 + 后端”改为“纯独立后端”方向：`obs-plugintemplate/` 已删除，nlohmann/json 已 vendor 到 `third_party/`。主程序 `single_hid_monitor`（入口 `main.cpp`）支持：枚举 HID 设备（`l`）、选择设备并自动检测全部按钮与轴值（`s N`）、打印配置（`p`）、保存/加载 profile（`w`）。后端自动检测与动态解析能力已完成。

## 待办（仅未来事项）

### F1 前端显示（独立窗口 + 图片合成）

- [ ] 实现 `OverlayWindow`：Win32 分层窗口（`WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST`），消费后端快照定时重绘
验收标准：窗口置顶透明穿透；消费 `HidOverlayState`（或动态扩展后的快照）；CPU 空闲占用低。

- [ ] 实现图片合成渲染（`ImageCompositor` + `AssetManager`）：预加载图片资源（按键常态/按下、旋钮帧、背景），按状态驱动合成
验收标准：资源按目录组织；按键按下/轴值变化对应画面正确切换。

- [ ] 确定前端 UI 技术栈（GDI+ / Direct2D / Dear ImGui / 其他）并落地最小可运行窗口
验收标准：技术栈确定且在 `run_build.cmd` 单一目标下可构建运行。

### F2 动态信号快照扩展

- [ ] 将快照从固定 7+1 字段扩展为动态变长按钮/轴数组，暴露全部自动检测信号
验收标准：`HidOverlayState`（或新结构）可承载任意数量的按钮与轴；自动检测到的信号全部可消费；同步更新 `publishState` 与语义文档。

- [ ] 确认入口/产物命名收敛（当前保留 `single_hid_monitor.exe`）
验收标准：如需改名 `2dx-overlay.exe`，同步更新 `run_build.cmd` 与文档；当前未改名即维持现状。

## 交接记录

- 当前接手时间：2026-09-20
- 本轮已完成（前置基础，非待办）：删除 `obs-plugintemplate/`；vendor `nlohmann/json` 到 `third_party/`；后端设备枚举、选择、自动检测、动态解析、profile 已实现；`main_standalone.cpp` 已重命名为 `main.cpp` 作为唯一入口。
- 本轮遗留风险：
  - `HidOverlayState` 仍为固定 7+1 字段，自动检测出的全部信号暂只在配置打印中体现，尚未全部进入快照（见 F2）。
  - 前端窗口/图片合成尚未实施（见 F1）。
- 下轮建议第一步：从 F1 前端显示的最小 Win32 窗口开始，先行打通“后端快照 -> 窗口显示”回路。
