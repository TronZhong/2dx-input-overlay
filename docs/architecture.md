# 架构设计文档

## 概览

2DX Input Overlay 采用分层模块化结构。`CMakeLists.txt` 将各目录源文件编译进单一可执行文件；目录分层表达职责，但并不完全等同于严格的依赖隔离。

---

## 分层详解

### Layer 0: core/ - 数据、能力枚举与 Profile

| 子模块 | 职责 | 关键类型/接口 |
|--------|------|---------------|
| `types/` | 核心数据结构 | `ButtonMapping`、`AxisMapping`、`MyHidConfig`、`MyHidState`、`HidOverlayState`、`DeviceInfo` |
| `profiles/` | 配置持久化 | `DeviceProfileManager`、`profile_serialization`（JSON读写及旧格式迁移） |
| `capabilities/` | HID 能力枚举 | `CapabilityInspector::enumerate()` → `HidCapabilities` |

**设计约束**：
- 当前 `hid_types.h` 直接包含 Windows/HID 头文件，`capabilities` 也调用 HID API，因此 `core/` 不是平台无关层。
- 映射使用动态 `buttons`/`axes` 容器；旧格式仅在 Profile 反序列化时兼容。
- 映射与状态容器通常按相同下标对应；报告解析失败的轴会保留该帧初始化的零值。

---

### Layer 1: platform/ - Windows 平台封装

| 子模块 | 职责 | 关键接口 |
|--------|------|----------|
| `enumerator/` | 系统HID设备枚举 | `DeviceEnumerator::enumerateAll()` → `vector<DeviceInfo>` |
| `raw_input/` | Raw Input 接收 | `RawInputWindow`(消息循环)、`RawInputRegistry::registerAll()`、`RawInputParser` |
| `hid/` | HID 报告 API 与 usage 名称 | `hid_parser::getActiveButtonUsages()`、`getUsageValue()`、`hid_usage_names::valueName()` |

**关键设计**：
- 通配注册（`usUsagePage=0, usUsage=0`）接收所有 HID，上层按 VID/PID 过滤
- `HidInputBackend::start()` 当前在调用线程创建 `RawInputWindow`，之后才启动 worker 并在 worker 上执行 `GetMessage` 循环。Win32 窗口消息归属创建窗口的线程；创建线程与泵消息线程不一致，因此不能据此认定 `WM_INPUT` 回调一定在 worker 上得到处理。见“线程模型”。
- 纯封装，无业务逻辑

---

### Layer 2: backend/ - 后端编排

| 组件 | 职责 |
|------|------|
| `HidInputBackend` | 公共 API：`start/stop/tryGetLatest/setTargetVidPid/autoConfigure/getConfig` |
| `DeviceManager` | 目标设备生命周期、多集合扫描/增删、capability 索引、所有权判定 |
| `StatePublisher` | 通过 mutex 保护最新状态快照 |
| `AutoConfig` | 自动检测编排：扫描→聚合 capabilities→`adapter.autoDetect()` |

**数据流**：
```
main() 创建 backend 并调用 start()
   │
   ▼
进入 CLI 命令循环；用户选择设备 s N
   │
   ├─▶ 有 profile：setTargetVidPid() 只设置 VID/PID，不应用 profile 映射
   │
   └─▶ 无 profile：autoConfigure() 在 CLI 线程扫描并生成映射
           ├─▶ DeviceManager.scanTargetDevices() → 所有匹配 VID/PID 的 handle
           ├─▶ 遍历 devices 并调用 CapabilityInspector::enumerate()
           ├─▶ 去重合并 buttons(usage+linkCollection) / values(usagePage+usage+linkCollection)
           └─▶ adapter.autoDetect(merged) → 生成 MyHidConfig.buttons/axes

RawInputWindow 创建于 CLI 调用线程，GetMessage 循环启动于 worker 线程
   │
   ▼
窗口消息处理（线程归属不匹配，当前实现不保证输入回调正常处理）
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
- 同一 VID/PID 的 Raw Input HID handle 被视为待监视集合
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
- 归一化：当 `logicalMax > logicalMin` 时 `clamp((raw - min) / (max - min), 0, 1)`，否则为 `0`
- 方向追踪（每轴独立 `AxisTrack`）：
  - `delta = raw - prevRaw`，环绕修正 `>128 → -256` / `<-128 → +256`
  - `delta>0` → `dir=1`，`delta<0` → `dir=-1`
  - `delta==0` → 99ms 内保持上一方向，否则 `0`
- 配置映射只将 `isRange==true` 的 Value Cap 作为轴；能力枚举本身仍会记录 Range 与 NotRange。
- 按钮解析以第一个按钮映射的 usage page/link collection 查询 active usages，再按 usage 比较映射项；不支持按映射项分别查询不同集合。

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
        if (!_kbhit() && (!hasLast || stateChanged(state, last))) {
            cout << formatLiveLine(config, state);
        }
    }
    // 2. 非阻塞命令输入
    if (_kbhit()) {
        line = getline();
        cmd = line[0];         args = line.substr(1);
        if (!args.empty() && args[0] == ' ') args = args.substr(1);
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

**实际依赖关系**：CLI 调用 backend、profiles 与 enumerator；backend 使用 adapter、capabilities 与 Raw Input；adapter 使用 platform HID parser；capabilities 直接使用 Windows HID API。目录结构有分层意图，但不能概括为 `core → platform → backend → adapter → cli` 的严格单向依赖。

---

## 线程模型

| 线程 | 职责 | 同步机制 |
|------|------|----------|
| CLI 调用线程 | 命令分发、状态轮询、调用 `autoConfigure`/`setTargetVidPid` | `tryGetLatest` 由 `StatePublisher::mutex` 保护 |
| Raw Input worker | `GetMessage` 循环；设计上处理窗口回调 | `StatePublisher::mutex` 保护发布；当前窗口是在调用线程创建，线程归属与消息循环线程不一致 |

**线程安全边界**：
- `configMutex` 只在 `getConfig`、`setTargetVidPid`、`autoConfigure` 中使用；Raw Input 回调访问 `config`、`adapter`、`deviceManager` 和 `masterState` 时不持有该锁。
- CLI 命令在 worker 启动后仍会从调用线程执行 `autoConfigure` 或 `setTargetVidPid`，而回调也可能访问同一组对象；这些对象没有共同同步机制，当前实现不能保证并发切换设备时无数据竞争。
- `StatePublisher` 的锁只保护其快照，不会使上述其他对象线程安全。

---

## 扩展点

| 扩展场景 | 修改位置 |
|----------|----------|
| 新增 HID Usage 名称 | `platform/hid/hid_usage_names.cpp` |
| 新增设备枚举字段 | `core/types/hid_types.h` + `platform/enumerator/` |
| 变更自动检测规则 | `backend/auto_config.cpp` + `adapter/config_mapper.cpp` |
| 新增 CLI 命令 | 继承 `Command`，在 `main.cpp` 注册 |
| 修改输出格式 | 对应 `cli/formatters/*.cpp` |
| 替换/增加消费端 | 消费 `HidOverlayState`；当前仓库只实现 CLI |