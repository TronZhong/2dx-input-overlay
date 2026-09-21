# DO-TODO List (AI Handoff)

最后更新时间：2026-09-21

## 现状摘要

仓库已从“OBS 插件 + 后端”改为“纯独立后端”方向：`obs-plugintemplate/` 已删除，nlohmann/json 已 vendor 到 `third_party/`。主程序 `single_hid_monitor`（入口 `main.cpp`）支持：枚举 HID 设备（`l`）、选择设备并自动检测全部按钮与轴值（`s N`）、打印配置（`p`）、保存/加载 profile（`w`）。后端自动检测与动态解析能力已完成，**但状态仅暴露固定 7+1 字段，终端只打印 7+1，看不到完整动态信号**。

## 待办（按优先级顺序）

### B-后端状态扩展与终端完整显示（核心交付，优先于 GUI）

- [ ] **B1 后端状态结构扩展**：`MyHidState`/`HidOverlayState` 增加动态数组字段
  - `MyHidState`：`dynamicButtons`/`dynamicAxesNorm`/`dynamicAxesRaw`/`dynamicAxesDir`（并行于 `config.buttons`/`config.axes`）
  - `HidOverlayState`：同步增加 `buttons`/`axesNorm`/`axesRaw`/`axesDir` 向量
  - 验收：编译通过；legacy 7+1 字段保留兼容

- [ ] **B2 解析器填充动态状态**：`updateFromReport` 遍历所有动态映射填充
  - 按钮：对每个 `config.buttons[i]` 判断是否按下 → `dynamicButtons[i]`
  - 轴：对每个 `config.axes[i]` 读取值 → 归一化/原始/方向写入对应数组
  - 兼容：前 7 按钮、第 1 轴仍同步写入 legacy 字段
  - 验收：动态映射时数组全量填充；无动态映射时数组为空、legacy 正常

- [ ] **B3 终端多行滚动完整打印**：`main.cpp` 循环改为多行滚动显示所有信号
  - `s N` 后每帧打印：`Btn: 1 0 0 1 ...  |  Axis: 0.42(107)↑  0.00(0)─ ...`
  - 列头来自 `config.buttons[i].name` / `config.axes[i].name+range`
  - 仅状态变化时刷新（可选减少噪音）
  - 验收：选中设备后终端持续滚动显示**所有**按钮与轴实时状态

- [ ] **B4 能力检测结果作为固定表头**（增强可读性）
  - `s N` 后先打印一次 `=== Capabilities ===` 摘要（按钮 usage/name、轴 usage/range/name）
  - 随后实时区滚动
  - 验收：表头固定、实时区独立滚动

---

### F1 前端显示（独立窗口 + 图片合成）—— 后置

- [ ] 实现 `OverlayWindow`：Win32 分层窗口（`WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST`），消费后端快照定时重绘
  验收标准：窗口置顶透明穿透；消费 `HidOverlayState`（动态扩展后的快照）；CPU 空闲占用低。

- [ ] 实现图片合成渲染（`ImageCompositor` + `AssetManager`）：预加载图片资源（按键常态/按下、旋钮帧、背景），按状态驱动合成
  验收标准：资源按目录组织；按键按下/轴值变化对应画面正确切换。

- [ ] 确定前端 UI 技术栈（GDI+ / Direct2D / Dear ImGui / 其他）并落地最小可运行窗口
  验收标准：技术栈确定且在 `run_build.cmd` 单一目标下可构建运行。

---

### F2 命名收敛（可选）

- [ ] 确认入口/产物命名收敛（当前保留 `single_hid_monitor.exe`）
  验收标准：如需改名 `2dx-overlay.exe`，同步更新 `run_build.cmd` 与文档；当前未改名即维持现状。

---

## 交接记录

- 当前接手时间：2026-09-21
- 本轮已完成（前置基础，非待办）：删除 `obs-plugintemplate/`；vendor `nlohmann/json` 到 `third_party/`；后端设备枚举、选择、自动检测、动态解析、profile 已实现；`main_standalone.cpp` 已重命名为 `main.cpp` 作为唯一入口。
- 本轮核心缺口：`HidOverlayState` 仍为固定 7+1 字段，自动检测出的全部信号暂只在配置打印中体现，尚未全部进入快照与终端实时显示（见 B1-B3）。
- 下轮建议第一步：从 **B1 后端状态结构扩展** 开始，按 B1→B2→B3→B4 顺序闭环“选设备→实时全量终端显示”。