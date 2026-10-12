# 数据流文档

## 核心数据结构流向

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           完整数据流图                                         │
└─────────────────────────────────────────────────────────────────────────────┘

用户输入 (CLI)
    │
    ▼
┌─────────────────┐     enumerateAll()     ┌──────────────────────┐
│  SelectCommand  │ ─────────────────────▶ │ DeviceEnumerator     │
│  (s N)          │ ◀───────────────────── │ (platform/enumerator)│
└─────────────────┘   vector<DeviceInfo>   └──────────────────────┘
    │
    │ user picks index N
    ▼
┌─────────────────┐     loadProfile()      ┌──────────────────────┐
│  SelectCommand  │ ─────────────────────▶ │ DeviceProfileManager │
│                 │ ◀───────────────────── │ (core/profiles)      │
└─────────────────┘   optional<Profile>    └──────────────────────┘
    │
    ├─▶ 有 Profile: ctx.config = profile.config
    │              backend.setTargetVidPid()
    │
    └─▶ 无 Profile: autoConfigure(vid, pid)
                │
                ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        AutoConfig::configure()                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. DeviceManager.scanTargetDevices()                                        │
│    - GetRawInputDeviceList → 所有 RIM_TYPEHID handle                        │
│    - 过滤 VID/PID 匹配 → tryAddTargetDevice()                               │
│    - loadPreparsedData() → 每个 handle 获取 preparsed data                  │
│    - indexDeviceCapabilities() → CapabilityInspector::enumerate()           │
│      输出: TargetHidDevice {handle, name, preparsed, buttonUsages, axisUsages}│
│                                                                              │
│ 2. 遍历所有 TargetHidDevice，聚合 capabilities                                │
│    - CapabilityInspector::enumerate(preparsed) → HidCapabilities            │
│    - 去重合并: buttons(usage+linkCollection), values(usagePage+usage+link)  │
│    - 输出: merged HidCapabilities (可选传给 CLI 打印表头)                   │
│                                                                              │
│ 3. adapter.autoDetect(merged) → config_mapper::mapCapabilitiesToConfig()    │
│    - buttons[]: usagePage=0x09, usage=b.usage, linkCollection=b.linkCollection│
│    - axes[]: 仅 isRange=true, usagePage=v.usagePage, usage=v.usage,         │
│      logicalMin/Max=v.logicalMin/Max, name=v.name                           │
│    - 写入 MyHidConfig.buttons/axes，index 连续分配                          │
│                                                                              │
│ 4. 返回 true + merged caps (可选)                                           │
└─────────────────────────────────────────────────────────────────────────────┘
    │
    ▼
ctx.config = backend.getConfig()  // 含动态映射
backend.start()  // 启动 Raw Input worker 线程
    │
    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Worker 线程 (RawInputWindow)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│ 消息循环: GetMessage → TranslateMessage → DispatchMessage                   │
│                                                                              │
│ WM_INPUT (lParam)                                                            │
│    │                                                                          │
│    ▼ RawInputParser::parseRawInput()                                         │
│    RawInputData {deviceHandle, rawData[], dataSize}                         │
│    │                                                                          │
│    ├─▶ 懒加载: DeviceManager.tryAddTargetDevice(handle)                     │
│    │                                                                          │
│    ▼ adapter.updateFromReport(preparsed, rawData, slice)                    │
│       │                                                                       │
│       ├─▶ button_parser::parseButtons()                                     │
│       │   - getActiveButtonUsages(usagePage, linkCollection) → activeUsages │
│       │   - 遍历 config.buttons[]，在 activeUsages 中查找 usage             │
│       │   - 输出: slice.dynamicButtons[] (bool vector)                      │
│       │                                                                       │
│       └─▶ axis_parser::parseAxes()                                          │
│           - 遍历 config.axes[]: getUsageValue() → raw                       │
│           - 归一化: (raw-min)/(max-min) clamp [0,1] → dynamicAxesNorm[]     │
│           - 方向追踪 AxisTrack: delta修正环绕、99ms空闲保持 → dynamicAxesDir│
│           - 输出: slice.dynamicAxesNorm/Raw/Dir[]                           │
│                                                                              │
│    │                                                                          │
│    ▼ DeviceManager.ownsButton/ownsAxis 判定所有权                            │
│       - 只有拥有该 usage 的集合才合并对应 slice 项                           │
│       - 防止跨集合清零 (如鼠标移动不清零键盘按键)                            │
│    │                                                                          │
│    ▼ 合并到 masterState (持久化，仅在映射尺寸变化时重分配)                    │
│    │                                                                          │
│    ▼ StatePublisher::publishState(masterState, config)                      │
│       - 复制 dynamicButtons/axesNorm/axesRaw/axesDir 到 HidOverlayState     │
│       - tickMs = GetTickCount64()                                            │
│       - mutex 保护 latest_，单生产者/单消费者                                │
└─────────────────────────────────────────────────────────────────────────────┘
    │
    ▼ (Main 线程轮询)
┌─────────────────┐     tryGetLatest()     ┌──────────────────────┐
│  main.cpp loop  │ ─────────────────────▶ │ StatePublisher       │
│                 │ ◀───────────────────── │ (HidOverlayState)    │
└─────────────────┘   bool + out state     └──────────────────────┘
    │
    ├─▶ 状态变化才打印 (live_formatter::stateChanged)
    │
    ▼ live_formatter::formatLiveLine(config, state)
    │
    ├─▶ Buttons: "Btn: [0]1(Btn 1) [1]0(Btn 2)..."
    │
    └─▶ Axes: "Axis: [0]=0.50(128)^(X) [1]=0.00(0)-(Y)"

---

## Profile 持久化流向

### 保存
```
WriteCommand (w [name])
    │
    ▼ profileMgr.saveProfile(config, name)
    │
    ▼ profile_serialization::configToJson(config)
    │   - vid/pid
    │   - buttons[]: {usagePage,usage,linkCollection,name,index}
    │   - axes[]: {usagePage,usage,linkCollection,logicalMin,logicalMax,name,index}
    │
    ▼ JSON 写入 profiles/<VID>_<PID>.json (pretty print)
```

### 加载
```
SelectCommand / 启动时
    │
    ▼ profileMgr.loadProfile(vid, pid)
    │
    ▼ 读取 profiles/<VID>_<PID>.json
    │
    ▼ profile_serialization::configFromJson(json)
    │   1) 有 buttons[]/axes[] → 直接反序列化 (新格式)
    │   2) 无动态映射但有 legacy 字段 → 重建:
    │      - buttonUsagePage + button_01Usage..07Usage + buttonLinkCollection
    │        → buttons[0..6]
    │      - axisUsagePage + xUsage + axisLinkCollection + xLogicalMin/Max
    │        → axes[0] (X), enableYAxis → axes[1] (Y)
    │
    ▼ 返回 DeviceProfile {vid,pid,name,config}
```

---

## 关键不变量

| 不变量 | 校验点 |
|--------|--------|
| `config.buttons.size() == state.dynamicButtons.size() == overlay.buttons.size()` | `updateFromReport` 分配、合并、发布时 |
| `config.axes.size() == state.dynamicAxes*.size() == overlay.axes*.size()` | 同上 |
| `masterState` 仅在 `config.buttons/axes` 尺寸变化时重分配 | `onRawInput` 合并前检查 |
| 每轴 `AxisTrack` 独立，互不干扰 | `axis_parser` 循环内 `axisTracks[i]` |
| Profile 仅写新格式，读兼容旧格式 | `profile_serialization.cpp` |

---

## 错误流向

| 场景 | 处理 | 结果 |
|------|------|------|
| 设备枚举单项失败 | `continue` | 不中断整体列表 |
| `GetRawInputData` 失败 | 返回 `false` | 丢弃该报告 |
| `HidP_GetUsages` 失败 | 返回 `false` | `dynamicButtons` 全 false |
| `HidP_GetUsageValue` 失败 | 返回 `false` | 该轴保持上一值/零 |
| Profile 读写失败 | `stderr` + 返回 `false`/`nullopt` | 不抛异常，CLI 提示用户 |
| Worker 线程异常退出 | `running=false` + `join()` | `stop()` 清理，主线程可重试 `start()` |