# DO-TODO List (AI Handoff)

最后更新时间：2026-09-21

## 现状摘要

仓库已从“OBS 插件 + 后端”改为“纯独立后端”方向：`obs-plugintemplate/` 已删除，nlohmann/json 已 vendor 到 `third_party/`。主程序 `single_hid_monitor`（入口 `main.cpp`）支持：枚举 HID 设备（`l`）、选择设备并自动检测全部按钮与轴值（`s N`）、打印配置（`p`）、保存/加载 profile（`w`）。后端自动检测与动态解析能力已完成；**B1-B4 已落地：状态结构扩展为动态数组，终端多行滚动实时显示所有按钮与轴。C1 已落地：支持复合 HID 设备（同一 VID/PID 的多 top-level 集合，如鼠标）的枚举聚合与实时解析合并，解决鼠标抓不到输入、自动检测报 `Buttons(0)` 的问题**。当前可完整满足“可自选已有连接 HID 设备 → 输出该设备全部状态（含复合设备多集合）”。

## 待办（按优先级顺序）

### B-后端状态扩展与终端完整显示（核心交付，优先于 GUI）✅ 已完成

- [x] **B1 后端状态结构扩展**：`MyHidState`/`HidOverlayState` 增加动态数组字段
  - `MyHidState`：`dynamicButtons`/`dynamicAxesNorm`/`dynamicAxesRaw`/`dynamicAxesDir`（并行于 `config.buttons`/`config.axes`）
  - `HidOverlayState`：同步增加 `buttons`/`axesNorm`/`axesRaw`/`axesDir` 向量
  - legacy 7+1 字段保留兼容；`run_build.cmd` 编译通过

- [x] **B2 解析器填充动态状态**：`updateFromReport` 遍历所有动态映射填充
  - 按钮：对每个 `config.buttons[i]` 用 `HidP_GetUsages` 全列表判定按下 → `dynamicButtons[i]`
  - 轴：对每个 `config.axes[i]` 读取 → 归一化/原始/方向写入对应数组
  - 新增 `trackAxisDirection(idx, raw, now)` 做每轴方向跟踪（含环绕修正与空闲保持），替代原单轴 `prevXRaw` 状态
  - 兼容：前 7 按钮、第 1 轴仍同步写入 legacy 字段；无动态映射时数组为空、legacy 正常

- [x] **B3 终端多行滚动完整打印**：`main.cpp` 循环改为多行滚动显示所有信号
  - 新增 `fmtTimestamp` / `stateChanged` / `formatLiveLine`；每帧打印 `[HH:MM:SS.mmm] connected=...`、`Btn: [i]0/1(name)`、`Axis: [i]=norm(raw)^|v|-(name)`
  - 动态映射存在时全量打印；否则回退 legacy 7+1
  - 输入命令时暂停实时输出避免交错；状态变化才刷新（减少噪音）

- [x] **B4 能力检测结果作为固定表头**（增强可读性）
  - `s N` 自动检测后已打印 `=== Detected Capabilities ===`（按钮 usage/name、轴 usage/range/name）
  - 实时行的按钮/轴项含 `config.buttons[i].name` / `config.axes[i].name`，与表头呼应

---

### C-复合 HID 设备（多 top-level 集合）聚合 — 进行中

**问题**：一个物理设备（如 Razer 鼠标 VID=0x1532 PID=0x0098）会暴露多个 top-level collection（Raw Input 各返回一个 handle，如 MI_00/MI_01、Col02/Col04/Col05）。旧代码只取第一个匹配集合，导致：
- Raw Input 只注册 0x04/0x05（Joystick/Gamepad），**鼠标（Usage=0x0）收不到 WM_INPUT** → `connected` 恒为 0，读不到输入。
- `autoConfigure` 只对第一个匹配 handle 枚举 → 该集合 Input caps 为 0 → 报 `Buttons(0)`，鼠标按钮抓不到。

**方案 C1（完整闭环）✅ 已完成**：

- [x] **C-A 通配注册**：`registerRawInput` 改为注册 `usUsagePage=0x00, usUsage=0x00` + `RIDEV_INPUTSINK|RIDEV_DEVNOTIFY`，接收所有 HID；设备是否处理仍由 `tryAddTargetDevice` 的 VID/PID 过滤决定（`hid_input_backend.cpp`）。
  验收：选中鼠标后不再恒为 `connected=0`；移动/点击/滚轮能触发 `onRawInput`。

- [x] **C-B 多集合枚举聚合**：`autoConfigure` 遍历 `devices` 中所有匹配 VID/PID 的 handle，分别 `CapabilityInspector::enumerate`，按 `usagePage+usage(+link)` 去重合并成一份 `HidCapabilities`，再 `adapter.autoDetect(合并结果)`（`hid_input_backend.cpp`）。
  验收：`Detected Capabilities` 显示真实按钮数与轴，不再是 `Buttons(0)`。

- [x] **C-C 多集合实时解析合并**：`TargetHidDevice` 记录各集合归属（`buttonUsages`/`axisUsages`，由新增 `indexDeviceCapabilities` 填充）；`onRawInput` 对每个 handle 用其自身 `preparsed` 独立解析出 `slice`，再按“所有权”合并到持久化的 `master` 状态（`hid_input_backend.cpp`）。仅在映射尺寸变化时重分配数组，避免跨集合清零。
  验收：移动鼠标 X/Y、点按钮、滚轮均能在终端实时反映。

- [x] **C-D 诊断打印**：`main.cpp` 新增 `printDeviceCollections`，`s N` 时列出选中 VID/PID 下所有集合的 `usagePage/usage/路径`，便于人工分辨。

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
- 前置基础已完成：删除 `obs-plugintemplate/`；vendor `nlohmann/json` 到 `third_party/`；后端设备枚举、选择、自动检测、动态解析、profile 已实现；`main_standalone.cpp` 已重命名为 `main.cpp` 作为唯一入口。
- 本轮已完成（B1-B4 + C1）：
  - B：状态结构动态化；`updateFromReport` 全量填充 + 每轴方向跟踪；终端多行滚动实时打印；能力表头。
  - C：Raw Input 通配注册接收所有 HID；`autoConfigure` 多集合枚举聚合（按 usage 去重）；`TargetHidDevice` 记录集合归属，`onRawInput` 按所有权把各集合解析结果合并到持久化 `master` 状态；`s N` 打印目标 VID/PID 下所有集合。
- 遗留风险 / 注意：
  - 交互式终端依赖真实控制台，重定向 stdin 时 `_kbhit`/`getline` 不生效（属预期，实机运行正常）。
  - 多集合合并假设“同一 usage 只属一个集合”；若某集合不周期性上报某 owned 按键，该键状态可能滞留。鼠标主集合（0x02）上报频繁，实际影响小。
  - `master` 在切换设备时由 `resetMasterState` 清空；断开最后一个设备时 `publishDisconnectedState` 覆盖 latest。
  - 需实机手动验证：选中鼠标后 `connected=1`、`Detected Capabilities` 显示真实按钮/轴、移动/点击/滚轮在终端反映。
- 下轮建议：实机验证 C1 效果后，进入 **F1 前端显示**（独立窗口 + 图片合成），消费已动态化的 `HidOverlayState`。