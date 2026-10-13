# DO-TODO List (AI Handoff) - 模块化重构版

最后更新时间：2026-10-13

## 现状摘要

项目已完成**模块化分层重构**：
- **core/** - 数据结构、Profile持久化、能力枚举（`hid_types.h` 与能力枚举依赖 Windows/HID SDK，并非零平台依赖）
- **platform/** - Windows Raw Input封装、设备枚举、HID解析API
- **backend/** - 后端编排：设备管理、状态发布、自动检测
- **adapter/** - Report解析：按钮/轴解析、配置映射
- **cli/** - 交互层：Command/Formatter模式

CLI 提供：`l`枚举设备、`s N`选择设备、`p`打印配置、`w`保存profile、`q`退出。无 profile 时会尝试自动检测；已加载的 profile 映射目前没有应用到 backend。Raw Input 窗口在调用线程创建，但消息循环在 worker 线程运行，窗口所属线程与消息泵不一致；因此不能把实时输入处理描述为已验证可用。

---

## 已完成阶段

### Phase 1: 核心类型层 ✅
- `core/types/hid_types.h` - 仅动态映射的ButtonMapping/AxisMapping/MyHidConfig/MyHidState/HidOverlayState/DeviceInfo
- `core/profiles/` - DeviceProfileManager + JSON序列化（含Legacy兼容迁移）
- `core/capabilities/` - CapabilityInspector枚举Button/Value Caps
- CMakeLists.txt按模块组织，source_group分组，移除legacy源文件

### Phase 2: 平台层 + Backend编排 ✅
- `platform/enumerator/` - DeviceEnumerator
- `platform/raw_input/` - RawInputWindow(消息循环)、RawInputRegistry(通配注册)、RawInputParser
- `platform/hid/` - hid_parser(GetUsages/GetUsageValue)、hid_usage_names
- `backend/` - HidInputBackend(公共API)、DeviceManager(多集合管理)、StatePublisher(mutex保护快照)、AutoConfig(自动检测编排)
- HidInputBackend含 worker 启动、WM_INPUT/WM_INPUT_DEVICE_CHANGE 处理、slice 合并和状态发布代码；窗口/消息泵线程安排及设备切换并发尚未验证可靠

### Phase 3: Adapter层 ✅
- `adapter/my_hid_adapter.h/.cpp` - 公共接口、配置持有、轴追踪状态
- `adapter/button_parser` - GetUsages → dynamicButtons
- `adapter/axis_parser` - GetUsageValue → 归一化/方向追踪
- `adapter/config_mapper` - HidCapabilities → MyHidConfig.buttons/axes

### Phase 4: CLI层 ✅
- Command模式：List/Select/Print/Write/QuitCommand
- Formatter模式：device/config/caps/live_formatter
- main.cpp组装Context、命令分发、状态轮询

---

## 待办（按优先级）

### P0: 功能验证与完善
- [ ] 实机验证 `s N` 选择设备 → 自动检测 → 实时显示按钮/轴
- [ ] 验证复合设备（鼠标）多集合合并：移动/点击/滚轮实时反映
- [ ] 验证Profile加载/保存循环：save → 重启 → select自动加载
- [ ] 验证切换设备时状态正确清零、新配置生效

### P1: 代码质量
- [ ] 补充单元测试：config_mapper、axis_parser方向追踪、profile迁移
- [x] 补充docs/architecture.md、data_flow.md、build.md
- [ ] 代码审查：const正确性、错误处理一致性、线程安全边界

### P2: 前端显示 - 当前不在项目范围
- 仓库当前定位为独立控制台监视程序；没有 OBS 插件或 Overlay 窗口开发任务。

---

## 架构对比

| 指标 | 重构前 | 重构后 |
|------|--------|--------|
| 单文件最大行数 | 663 (hid_input_backend.cpp) | ~200 |
| 模块数 | 扁平8文件 | 5层20+文件 |
| 文档覆盖 | 根目录4个.md | 每模块AGENTS.md+业务.md |
| 依赖方向 | 循环/混乱 | 按目录分层；core 中部分代码直接依赖 Windows/HID API |
| Legacy字段 | 混合在MyHidConfig | 彻底移除，仅动态映射 |

---

## 关键技术点

1. **多集合合并**：AutoConfig聚合所有同VID/PID集合的caps去重；运行时按ownership合并slice
2. **并发限制**：StatePublisher mutex仅保护发布快照；配置切换、报告处理和设备管理对象没有统一同步
3. **Legacy兼容**：profile_serialization读旧JSON自动重建buttons[]/axes[]；存盘仅写新格式
4. **方向追踪**：每轴独立AxisTrack，环绕修正(>128/-128)，99ms空闲保持
5. **Command/Formatter**：CLI可扩展、可测试、关注点分离