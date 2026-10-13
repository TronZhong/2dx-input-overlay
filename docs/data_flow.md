# 数据流文档

## 核心数据结构流向

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           完整数据流图                                         │
└─────────────────────────────────────────────────────────────────────────────┘

`main()` 启动时以默认配置创建 backend，并尝试加载默认 VID/PID 的 Profile。
若加载成功，仅更新 `ctx.config`；当前没有把 Profile 配置传入 backend。
backend 在命令循环前启动，之后才会处理用户输入的 `s N`。

`s N` 先刷新设备列表并取得选中项的 VID/PID，再按以下分支处理：

┌─────────────────┐     loadProfile()      ┌──────────────────────┐
│  SelectCommand  │ ─────────────────────▶ │ DeviceProfileManager │
│                 │ ◀───────────────────── │ (core/profiles)      │
└─────────────────┘   optional<Profile>    └──────────────────────┘
    │
    ├─▶ 有 Profile: ctx.config = profile.config
    │              backend.setTargetVidPid()（只更新 VID/PID，不应用映射）
    │
    └─▶ 无 Profile: backend.autoConfigure(vid, pid)
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
│ 4. 只要任一枚举结果 valid=true 就返回 true + merged caps                    │
│    即使映射为空或 adapter.autoDetect() 返回 false，当前也不会令流程失败     │
└─────────────────────────────────────────────────────────────────────────────┘
    │
    └─▶ 自动检测成功时，CLI 再从 backend.getConfig() 更新 ctx.config

┌─────────────────────────────────────────────────────────────────────────────┐
│                        Raw Input 消息处理                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ 当前窗口在 CLI 调用线程创建，worker 随后运行 GetMessage。                    │
│ Win32 窗口消息归属创建线程；此线程安排不保证窗口回调由 worker 正常泵送。    │
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
│       - tickMs = GetTickCount64()（旧目标版本宏下回退到 GetTickCount）       │
│       - mutex 仅保护 StatePublisher 的快照                                   │
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

```

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
| 成功解析报告时，slice 动态数组按当前映射尺寸分配 | `updateFromReport`；对应 HID 读取失败的轴为零值 |
| 已连接快照复制 masterState 当前数组；断开快照的数组为空 | `StatePublisher::publishState` / `publishDisconnectedState` |
| `masterState` 仅在 `config.buttons/axes` 尺寸变化时重分配 | `onRawInput` 合并前检查 |
| 每轴 `AxisTrack` 独立，互不干扰 | `axis_parser` 循环内 `axisTracks[i]` |
| Profile 仅写新格式，读兼容旧格式 | `profile_serialization.cpp` |

---

## 错误流向

| 场景 | 处理 | 结果 |
|------|------|------|
| 设备枚举单项失败 | `continue` | 不中断整体列表 |
| `GetRawInputData` 失败 | 返回 `false` | 丢弃该报告 |
| `HidP_GetUsages` 失败 | wrapper 返回 `false`，调用方未检查，usage 列表保留 64 个零值 | 常规非零 usage 映射为 false；usage 为 0 的映射可能匹配零值 |
| `HidP_GetUsageValue` 失败 | wrapper 返回 `false`，轴解析器跳过该轴 | 本帧该轴 raw/norm/dir 为预分配的零值 |
| Profile 读取/解析异常 | `stderr` + `nullopt` | CLI 将其视为未找到 Profile |
| Profile 保存异常 | `stderr` + `false` | CLI 输出保存失败；但当前未检查 ofstream 最终状态 |
| worker 线程退出 | 消息循环结束后线程可 join | 当前未设置异常捕获或异常退出恢复策略 |

`live_formatter::fmtTimestamp()` 使用当前本地时间；尽管快照带有 `tickMs`，格式化器当前不使用它。