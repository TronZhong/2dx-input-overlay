# DO-TODO List (AI Handoff)

最后更新时间：2026-04-27

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
- [x] 确认 `scripts/fetch-deps.ps1` 已加入单进程锁（`.fetch-deps.lock`）与失败文件清单输出
- [x] 确认 `scripts/fetch-deps.ps1` 现按 `buildspec.json` 派生依赖目标，`obs-deps` 包改走 GitHub API asset URL 并优先使用 `curl.exe` 下载
- [x] 确认已补充 `HidOverlayState` 字段语义文档与最小回归验证文档
- [x] 确认 OBS 子项目工具链口径存在不一致：`README.md` 标注 VS2022，但 `CMakePresets.json` 使用 `Visual Studio 18 2026`
- [x] 确认 `obs-plugintemplate/CMakePresets.json` 已去除本机路径硬编码；`cmake --preset windows-x64` 可进入依赖下载阶段，不再因 `D:/Tools/vs` 缺失直接失败
- [x] 确认最新一次 `fetch-deps.ps1 -MaxRetry 1` 已进入真实下载并曾推进到 `windows-deps-2025-07-11-x64.zip` 约 87%，但离线预取尚未完整跑通

## P0（优先处理）

- [x] 恢复 OBS 依赖下载链路并完成离线预取
验收标准：`.deps/windows-deps-2025-07-11-x64.zip`、`.deps/windows-deps-qt6-2025-07-11-x64.zip`、`.deps/31.1.1.zip` 三文件均存在且 SHA256 分别匹配 `buildspec.json`。

- [x] 清理下载并发占用并固化单进程下载流程
验收标准：执行下载脚本前后 `Get-Process curl` 不存在残留占用；下载过程不再出现 `Permission denied`、`File in use`。

- [x] 补齐并验证 `obs-plugintemplate/scripts/fetch-deps.ps1` 可复用脚本
验收标准：单条命令 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1` 能完成“下载+哈希校验”，失败时输出明确失败文件名并返回非 0。
标准执行命令：`Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate; powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1 -BuildAfter`

- [x] 记录并固化依赖下载失败后的清理重试步骤
验收标准：`OBS_MINIMAL_LOAD_STEPS.md`、`MINIMAL_REGRESSION_CHECKLIST.md`、交接记录对 `.deps/.fetch-deps.lock`、`.deps/*.part` 的清理与重跑命令描述一致，后续接手者无需再从终端历史反推。

- [x] 在依赖就绪后完成一次 OBS 子项目最小构建贯通
验收标准：`cmd /c obs-plugintemplate/run_build.cmd` 至少一次完整通过，且不再在 `buildspec_common.cmake:181/187` 失败。

- [x] 统一 OBS 子项目工具链版本口径
验收标准：`README.md`、`OBS_MINIMAL_LOAD_STEPS.md`、`obs-plugintemplate/CMakePresets.json` 对 Windows 构建工具链版本描述一致，并完成一次同口径下的 `run_build.cmd` 验证。

- [x] 去除或参数化 `CMAKE_GENERATOR_INSTANCE` 的本机绝对路径
验收标准：`obs-plugintemplate/CMakePresets.json` 不再强依赖 `D:/Tools/vs`；在非该路径环境执行 `cmake --preset windows-x64` 可进入正常配置流程（即不因实例路径不存在而直接失败）。

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
- [x] 用真实设备参数补全 `DEVICE_INFO_TEMPLATE.md`
验收标准：VID/PID、7 按钮 usage、X 轴 usage 与 logical range 完整且可复现。

- [x] 将 `my_hid_adapter` 的解析逻辑与设备日志结果逐项对齐
验收标准：按键/方向输出与设备真实输入一致，无明显漏判与误判。

- [x] 完成 OBS Source 侧输入语义核对（7 键 + `xDirection`）
验收标准：`input-overlay-source.c` 渲染状态与后端快照字段一一对应，无语义漂移。

## P2（OBS 子项目）

- [x] 检查 `obs-plugintemplate` 与主工程的桥接边界（数据结构/线程模型）
验收标准：确认“采集线程”与“渲染线程”间只传递快照数据，不在渲染线程直接调用 HID API。

- [x] 在具备 VS + Windows SDK 的环境中验证 `obs-plugintemplate` 最小构建
验收标准：至少一次本地可编译通过，记录所需工具链版本。

- [x] 补充一份最小运行步骤文档（从编译到在 OBS 中加载）
验收标准：其他人按文档可完成首次加载验证。

- [x] 验证 OBS 中热插拔与断连状态显示
验收标准：设备断开时显示状态可回落，重连后可恢复实时更新。

## 质量与可维护性

- [x] 增加基础自检清单（设备热插拔、断连、无输入时 CPU 占用）
验收标准：每项都有“步骤 + 期望结果 + 实测结果”。

- [ ] 为关键模块补充注释与边界说明（尤其是 HID 解析与线程退出）
验收标准：新接手者 15 分钟内能讲清主流程。

- [x] 给 `HidOverlayState` 的字段定义补一份输入语义文档
验收标准：字段来源、取值范围、刷新频率清晰可查。

- [x] 形成最小回归验证脚本（文档级）
验收标准：包含“主工程构建运行 + 设备输入验证 + OBS 最小加载验证”三段可复现步骤。

## 交接记录（给下一个 AI）

- [x] 当前接手时间：2026-04-27
- [x] 接手模型与环境：GPT-5.4 / Windows / VS Code
- [x] 本轮改动文件：`README.md`、`DO_TODO_LIST.md`、`obs-plugintemplate/CMakePresets.json`、`obs-plugintemplate/scripts/fetch-deps.ps1`、`OBS_MINIMAL_LOAD_STEPS.md`、`MINIMAL_REGRESSION_CHECKLIST.md`
- [x] 本轮完成项（勾选上方对应条目）：去除 `CMAKE_GENERATOR_INSTANCE` 本机绝对路径依赖；统一 `README.md` 与 `OBS_MINIMAL_LOAD_STEPS.md` 的 Windows 工具链口径；补强 `scripts/fetch-deps.ps1` 的依赖下载链路；补齐依赖下载中断后的文档化重试步骤
- [ ] 本轮遗留风险：当前主线功能与构建已闭环，后续如网络再抖动需按既有重试流程处理 GitHub 连接间歇失败
- [x] 下轮建议第一步：如进入下一迭代，优先处理 Future 区域事项（spice2x 可行性归档），避免打断已稳定主线

- [x] 本轮补充：`scripts/fetch-deps.ps1` 现按 `buildspec.json` 派生目标，`obs-deps` 包通过 GitHub API asset URL 绕过不稳定的 `github.com` 首跳，并优先使用 `curl.exe` 下载；下载被中断后需清理 `.deps/.fetch-deps.lock` 与 `.deps/*.part` 再重试
- [x] 本轮补充：依赖三文件已全部下载并哈希 `PASS`，`cmd /c run_build.cmd` 已出现 `[OK] OBS plugin build completed via preset windows-x64.`，当前源码编译阻塞已解除
- [x] 本轮补充：默认参数下已完成设备输入与 OBS 热插拔实测；`DEVICE_INFO_TEMPLATE.md`、`MINIMAL_REGRESSION_CHECKLIST.md` 已补齐实测记录
