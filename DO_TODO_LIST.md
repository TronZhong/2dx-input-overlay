# DO-TODO List (AI Handoff)

最后更新时间：2026-04-22

## 编写格式规范（给后续 AI）

- [ ] 所有可执行事项必须使用 `- [ ]` 或 `- [x]` 开头，不使用其他符号。
- [ ] 每条事项只写一件事，避免“并且/同时”导致无法独立勾选。
- [ ] 状态快照区只记录“已被证据确认”的事实，不写猜测。
- [ ] 事项按优先级分区：`P0`（阻塞项）-> `P1`（核心推进项）-> `P2`（增强项）。
- [ ] 每条事项下方必须有一行 `验收标准：...`，描述可验证结果。
- [ ] `验收标准` 必须可观察、可复现，避免“差不多可用”这类主观描述。
- [ ] 文档新增事项时，保持原有分区顺序，不打乱既有条目。
- [ ] 修改已有事项时，优先更新原条目，不重复新增同义条目。
- [ ] 若事项被阻塞，在条目末尾追加 `(阻塞)`，并在交接记录写明阻塞原因。
- [ ] 路径、命令、文件名统一用反引号包裹，例如 `build/Release/single_hid_monitor.exe`。
- [ ] 每轮交接前，至少更新一次“交接记录”中的完成项与遗留风险。
- [ ] 不删除历史结构（分区标题、交接记录区）；仅做增量更新。

## 项目状态快照（已确认）

- [x] 确认这是一个 Windows 下的 CMake/C++ HID 输入示例工程（目标程序：single_hid_monitor）
- [x] 确认主工程关键文件：`main.cpp`、`hid_input_backend.cpp/.h`、`my_hid_adapter.cpp/.h`
- [x] 确认当前可构建：`cmake -S . -B build` + `cmake --build build --config Release` 已通过
- [x] 确认产物路径：`build/Release/single_hid_monitor.exe`
- [x] 确认源码中未检出 `TODO/FIXME/HACK/XXX` 标记（需后续人工复核一次）
- [x] 确认仓库存在较多构建产物目录：`build/`、`build_x64_v18/`、`build-vs/`、`build-probe/`
- [x] 确认 `obs-plugintemplate` 已完成最小 Source 骨架与后端桥接（`input-overlay-source.c`、`hid-backend-bridge.cpp`）
- [x] 确认 `obs-plugintemplate` 已从误提交的 gitlink 转为主仓库普通目录（不使用 submodule）
- [x] 确认当前输入语义以“7 按钮 + 1 个 X 轴方向”为主，不再按“Start/Service + X/Y”描述
- [x] 确认已执行一次本地构建产物清理（`main.exe` 与历史 `build*` 目录）
- [x] 确认 `obs-plugintemplate/run_build.cmd` 已改为 CMake Preset（`windows-x64`）路径，SDK 版本门槛误报已消除
- [x] 确认当前 OBS 构建阻塞点在依赖下载阶段（`cmake/common/buildspec_common.cmake:181`），非源码编译错误
- [x] 确认网络连通性表现不一致：`github.com:443` 不稳定，`codeload.github.com`/`release-assets.githubusercontent.com:443` 可连通
- [x] 确认 `.deps` 当前存在不完整依赖包（0 字节或哈希不匹配）与下载进程占用冲突（`curl: (23) Permission denied`）

## P0（优先处理）

- [ ] 恢复 OBS 依赖下载链路并完成离线预取（阻塞）
验收标准：`.deps/windows-deps-2025-07-11-x64.zip`、`.deps/windows-deps-qt6-2025-07-11-x64.zip`、`.deps/31.1.1.zip` 三文件均存在且 SHA256 分别匹配 `buildspec.json`。

- [ ] 清理下载并发占用并固化单进程下载流程（阻塞）
验收标准：执行下载脚本前后 `Get-Process curl` 不存在残留占用；下载过程不再出现 `Permission denied`、`File in use`。

- [ ] 补齐并验证 `obs-plugintemplate/scripts/fetch-deps.ps1` 可复用脚本（阻塞）
验收标准：单条命令 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1` 能完成“下载+哈希校验”，失败时输出明确失败文件名并返回非 0。
标准执行命令：`Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate; powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1 -BuildAfter`

- [ ] 在依赖就绪后完成一次 OBS 子项目最小构建贯通（阻塞）
验收标准：`cmd /c obs-plugintemplate/run_build.cmd` 至少一次完整通过，且不再在 `buildspec_common.cmake:181/187` 失败。

- [x] 将根目录文档统一为“内部推进”口径（先不面向用户）
验收标准：`README.md`、`DEVICE_INFO_TEMPLATE.md`、`SPICE2X_INTEGRATION_NOTES.md` 均不出现用户导向文案，且与代码现状一致。

- [x] 评估并更新 `.gitignore`，避免提交大体积构建产物
验收标准：`git status` 不再被大量自动生成文件污染。

- [x] 清理并统一构建目录策略（建议仅保留一个主构建目录）
验收标准：构建说明、目录约定与实际执行路径一致，不再出现多套目录并行导致的混淆。

- [x] 收敛“保留必要文档”清单并冻结文档边界
验收标准：根目录仅保留推进必须文档（工程说明、待办、设备参数模板、接入说明），其余内容不再扩散。

## P1（功能与集成）

- [ ] 用真实设备参数补全 `DEVICE_INFO_TEMPLATE.md`
验收标准：VID/PID、7 按钮 usage、X 轴 usage 与 logical range 完整且可复现。

- [ ] 将 `my_hid_adapter` 的解析逻辑与设备日志结果逐项对齐
验收标准：按键/方向输出与设备真实输入一致，无明显漏判与误判。

- [ ] 完成 OBS Source 侧输入语义核对（7 键 + `xDirection`）
验收标准：`input-overlay-source.c` 渲染状态与后端快照字段一一对应，无语义漂移。

## P2（OBS 子项目）

- [x] 检查 `obs-plugintemplate` 与主工程的桥接边界（数据结构/线程模型）
验收标准：确认“采集线程”与“渲染线程”间只传递快照数据，不在渲染线程直接调用 HID API。

- [ ] 在具备 VS + Windows SDK 的环境中验证 `obs-plugintemplate` 最小构建
验收标准：至少一次本地可编译通过，记录所需工具链版本。

- [ ] 补充一份最小运行步骤文档（从编译到在 OBS 中加载）
验收标准：其他人按文档可完成首次加载验证。

- [ ] 验证 OBS 中热插拔与断连状态显示
验收标准：设备断开时显示状态可回落，重连后可恢复实时更新。

## Future（冻结项）

- [ ] 评估 spice2x 集成可行性（仅归档，不进入当前迭代）
验收标准：仅更新 `SPICE2X_INTEGRATION_NOTES.md` 的风险与前置条件，不改动主线实现。

## 质量与可维护性

- [ ] 增加基础自检清单（设备热插拔、断连、无输入时 CPU 占用）
验收标准：每项都有“步骤 + 期望结果 + 实测结果”。

- [ ] 为关键模块补充注释与边界说明（尤其是 HID 解析与线程退出）
验收标准：新接手者 15 分钟内能讲清主流程。

- [ ] 给 `HidOverlayState` 的字段定义补一份输入语义文档
验收标准：字段来源、取值范围、刷新频率清晰可查。

- [ ] 形成最小回归验证脚本（文档级）
验收标准：包含“主工程构建运行 + 设备输入验证 + OBS 最小加载验证”三段可复现步骤。

## 交接记录（给下一个 AI）

- [x] 当前接手时间：2026-04-22
- [x] 接手模型与环境：GPT-5.3-Codex / Windows / VS Code
- [x] 本轮改动文件：`README.md`、`DO_TODO_LIST.md`、`obs-plugintemplate/run_build.cmd`
- [x] 本轮完成项（勾选上方对应条目）：完成 gitlink 修复；确认 SDK 门槛误报已解除；定位当前阻塞为依赖下载与文件占用冲突
- [ ] 本轮遗留风险：OBS 依赖包下载仍不稳定，`.deps` 内存在不完整文件，导致 OBS 子项目构建未贯通
- [x] 下轮建议第一步：先执行“清理进程占用 + 依赖哈希校验 + 缺失包重下”，再执行 `obs-plugintemplate/run_build.cmd`

- [x] 本轮补充：已提供 `scripts/fetch-deps.ps1` 的标准执行命令，供下次 AI 直接复用
