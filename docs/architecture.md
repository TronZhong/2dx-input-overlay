# 架构设计文档

## 概览

2DX Input Overlay 采用 **分层模块化架构**，核心原则：
- **单向依赖**：core → platform → backend → adapter → cli
- **职责分离**：每层只做一件事，接口薄、实现内聚
- **文档同层**：每模块含 `AGENTS.md` + 业务 `.md`，代码旁即文档

---

## 分层详解

### Layer 0: core/ - 纯数据与业务逻辑（零平台依赖）

| 子模块 | 职责 | 关键类型/接口 |
|--------|------|---------------|
| `types/` | 核心数据契约 | `ButtonMapping`、`AxisMapping`、`MyHidConfig`、`MyHidState`、`HidOverlayState`、`DeviceInfo` |
| `profiles/` | 配置持久化 | `DeviceProfileManager`、`profile_serialization`（JSON读写+Legacy迁移） |
| `capabilities/` | HID能力枚举 | `CapabilityInspector::enumerate()` → `HidCapabilities` |

**设计约束**：
- 仅 STL + Windows 类型别名（`HANDLE`/`USAGE` 等），无 `<windows.h>` 逻辑
- 仅动态映射：移除所有 legacy 固定字段（7按键+1轴）
- 不变量：`config.buttons.size() == state.dynamicButtons.size() == overlay.buttons.size()`

---

### Layer 1: platform/ - Windows 平台封装

| 子模块 | 职责 | 关键接口 |
|--------|------|----------|
| `enumerator/` | 系统HID设备枚举 | `DeviceEnumerator::enumerateAll()` → `vector<DeviceInfo>` |
| `raw_input/` | Raw Input 接收 | `RawInputWindow`(消息循环)、`RawInputRegistry::registerAll()`、`RawInputParser` |
| `hid/` | HidP_* API 封装 | `hid_parser::getActiveButtonUsages()`、`getUsageValue()`、`hid_usage_names::valueName()` |

**关键设计**：
- 通配注册（`usUsagePage=0, usUsage=0`）接收所有 HID，上层按 VID/PID 过滤
- 消息循环跑在独立线程，回调也在该线程
- 纯封装，无业务逻辑

---

### Layer 2: backend/ - 后端编排

| 组件 | 职责 |
|------|------|
| `HidInputBackend` | 公共 API：`start/stop/tryGetLatest/setTargetVidPid/autoConfigure/getConfig` |
| `DeviceManager` | 目标设备生命周期、多集合扫描/增删、capability 索引、所有权判定 |
| `StatePublisher` | 线程安全发布（mutex 单生产者/单消费者） |
| `AutoConfig` | 自动检测编排：扫描→聚合 capabilities→`adapter.autoDetect()` |

**数据流**：
```
用户 s N
   │
   ▼
HidInputBackend::autoConfigure(vid,pid)
   │
   ├─▶ DeviceManager.scanTargetDevices() → 所有匹配 VID/PID 的 handle
   │       └─▶ tryAddTargetDevice() → loadPreparsedData + indexDeviceCapabilities
   │
   ├─▶ AutoConfig.configure()
   │       ├─▶ 遍历 devices，每个 preparsed 调 CapabilityInspector::enumerate()
   │       ├─▶ 去重合并 buttons(values+linkCollection) / values(usagePage+usage+linkCollection)
   │       ├─▶ adapter.autoDetect(merged) → 生成 MyHidConfig.buttons/axes
   │       └─▶ 可选输出 merged caps 供 CLI 打印
   │
   ▼
start() → RawInputWindow + 通配注册 → 消息循环线程
   │
   ▼
WM_INPUT 回调
   │
   ├─▶ RawInputParser::parseRawInput() → deviceHandle + rawData
   ├─▶ 懒加载 tryAddTargetDevice()
   ├─▶ adapter.updateFromReport(preparsed, rawData, slice)
   ├─▶ DeviceManager.ownsButton/ownsAxis 判定所有权
   ├─▶ 合并 slice 到 master state
   ├─▶ StatePublisher::publishState(master, config)
   │
   ▼
CLI tryGetLatest() 读取最新快照
```

**多集合合并策略**：
- 复合设备（鼠标+键盘、手柄+音频）在 Raw Input 中表现为多 handle
- `autoConfigure` 聚合所有集合 capabilities（去重）
- 运行时每集合用自己的 preparsed 解析，按 ownership 合并到 master state
- `TargetHidDevice.buttonUsages/axisUsages` 记录各集合拥有的信号，防止跨集合清零

---

### Layer 3: adapter/ - Report 解析

| 组件 | 职责 |
|------|------|
| `MyHidAdapter` | 公共接口、持有配置、轴追踪状态 `axisTracks_` |
| `button_parser` | `HidP_GetUsages` → `dynamicButtons[]` |
| `axis_parser` | `HidP_GetUsageValue` → 归一化 + 方向追踪 |
| `config_mapper` | `HidCapabilities` → `MyHidConfig.buttons/axes` |

**关键算法**：
- 归一化：`clamp((raw - min) / (max - min), 0, 1)`
- 方向追踪（每轴独立 `AxisTrack`）：
  - `delta = raw - prevRaw`，环绕修正 `>128 → -256` / `<-128 → +256`
  - `delta>0` → `dir=1`，`delta<0` → `dir=-1`
  - `delta==0` → 99ms 内保持上一方向，否则 `0`
- 仅 `isRange==true` 的 Value Cap 视为轴

---

### Layer 4: cli/ - 交互层

| 模式 | 组件 |
|------|------|
| Command | `ListCommand`/`SelectCommand`/`PrintCommand`/`WriteCommand`/`QuitCommand` |
| Formatter | `device_formatter`/`config_formatter`/`caps_formatter`/`live_formatter` |

**主循环**：
```cpp
while (ctx.running) {
    // 1. 轮询后端
    if (backend.tryGetLatest(state)) {
        if (!kbhit() && stateChanged(state, last)) {
            cout << formatLiveLine(config, state);
        }
    }
    // 2. 非阻塞命令输入
    if (kbhit()) {
        line = getline();
        cmd = line[0]; args = line.substr(1);
        commands[cmd]->execute(ctx, args);
    }
    Sleep(5);
}
```

---

## 依赖图

```
cli (main.cpp)
    │
    ├─▶ commands/*.cpp → CliContext
    │       ├─▶ backend/HidInputBackend
    │       ├─▶ core/profiles/DeviceProfileManager
    │       └─▶ platform/enumerator/DeviceEnumerator
    │
    └─▶ formatters/*.cpp → 纯格式化，无副作用

backend/HidInputBackend
    ├─▶ backend/DeviceManager
    │       ├─▶ platform/raw_input/RawInputParser
    │       └─▶ core/capabilities/CapabilityInspector
    ├─▶ backend/StatePublisher
    ├─▶ backend/AutoConfig
    │       └─▶ adapter/MyHidAdapter.autoDetect()
    │               └─▶ adapter/config_mapper
    └─▶ adapter/MyHidAdapter
            ├─▶ adapter/button_parser → platform/hid/hid_parser
            └─▶ adapter/axis_parser   → platform/hid/hid_parser
```

**单向依赖**：core → platform → backend → adapter → cli（无反向、无循环）

---

## 线程模型

| 线程 | 职责 | 同步机制 |
|------|------|----------|
| Main (CLI) | 命令分发、状态轮询、格式化输出 | 无锁读 `tryGetLatest()` |
| Worker (Raw Input) | 窗口消息循环、`WM_INPUT`/`WM_INPUT_DEVICE_CHANGE` 处理 | `configMutex` 保护配置变更；`StatePublisher::mutex` 保护发布 |

**线程安全边界**：
- `DeviceManager` 仅在 Worker 线程访问，无锁
- `MyHidAdapter.axisTracks_` 仅在 Worker 线程 `updateFromReport` 修改（`mutable`）
- 配置变更（`setTargetVidPid`/`autoConfigure`）在 Main 线程，持有 `configMutex`

---

## 扩展点

| 扩展场景 | 修改位置 |
|----------|----------|
| 新增 HID Usage 名称 | `platform/hid/hid_usage_names.cpp` |
| 新增设备枚举字段 | `core/types/hid_types.h` + `platform/enumerator/` |
| 变更自动检测规则 | `backend/auto_config.cpp` + `adapter/config_mapper.cpp` |
| 新增 CLI 命令 | 继承 `Command`，在 `main.cpp` 注册 |
| 修改输出格式 | 对应 `cli/formatters/*.cpp` |
| 替换前端渲染 | 消费 `HidOverlayState`，无需动后端 |