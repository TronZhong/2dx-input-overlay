# HidCapabilities / CapabilityInspector

文件位置：`core/capabilities/hid_capabilities.h/.cpp`

## 职责

从设备的 `PHIDP_PREPARSED_DATA`（HID 预解析数据）枚举**所有** Input 报告中的：
- 按钮 Usage（`HidP_GetButtonCaps`）
- 值/轴 Usage（`HidP_GetValueCaps`，仅 `IsRange==true` 视为轴）

不做任何过滤、去重或业务判断，原始返回供 `config_mapper` 转换为 `ButtonMapping`/`AxisMapping`。

---

## 数据结构

### DetectedButton

| 字段 | 来源 | 含义 |
|------|------|------|
| `usage` | `HIDP_BUTTON_CAPS.NotRange.Usage` 或 `Range.UsageMin..Max` | Button Page (0x09) 上的 Usage ID |
| `linkCollection` | `HIDP_BUTTON_CAPS.LinkCollection` | 所属 Link Collection |
| `name` | 自动生成 `"Btn " + usage` | 供显示，后续 `config_mapper` 可覆盖 |

### DetectedValue

| 字段 | 来源 | 含义 |
|------|------|------|
| `usagePage` | `HIDP_VALUE_CAPS.UsagePage` | 如 0x01 Generic Desktop |
| `usage` | `NotRange.Usage` 或 `Range.UsageMin..Max` | Usage ID |
| `linkCollection` | `HIDP_VALUE_CAPS.LinkCollection` | 所属 Link Collection |
| `logicalMin`/`logicalMax` | `HIDP_VALUE_CAPS.LogicalMin/Max` | 逻辑范围，归一化分母 |
| `isRange` | `LogicalMax > LogicalMin` | `true` 视为轴，`false` 视为标量（当前仅保留 `true`） |
| `name` | `valueName(usagePage, usage)` | 通用桌面页有语义名，否则十六进制 |

### HidCapabilities

| 字段 | 含义 |
|------|------|
| `valid` | `true` 表示至少成功调用过一次枚举 API |
| `buttons` | 所有检测到的按钮（展开 Range 后的单个 Usage） |
| `values` | 所有检测到的范围值（展开 Range 后的单个 Usage） |

---

## 枚举算法

```cpp
HidCapabilities enumerate(PHIDP_PREPARSED_DATA preparsed) {
    // 1. Button Caps
    USHORT btnLen = 0;
    HidP_GetButtonCaps(HidP_Input, nullptr, &btnLen, preparsed);
    vector<HIDP_BUTTON_CAPS> btnCaps(btnLen);
    HidP_GetButtonCaps(HidP_Input, btnCaps.data(), &btnLen, preparsed);
    for (c : btnCaps) {
        if (c.IsRange) {
            for (u = c.Range.UsageMin; u <= c.Range.UsageMax; ++u)
                push DetectedButton{u, c.LinkCollection, "Btn "+u};
        } else {
            push DetectedButton{c.NotRange.Usage, c.LinkCollection, "Btn "+c.NotRange.Usage};
        }
    }

    // 2. Value Caps
    USHORT valLen = 0;
    HidP_GetValueCaps(HidP_Input, nullptr, &valLen, preparsed);
    vector<HIDP_VALUE_CAPS> valCaps(valLen);
    HidP_GetValueCaps(HidP_Input, valCaps.data(), &valLen, preparsed);
    for (c : valCaps) {
        if (c.IsRange) {
            for (u = c.Range.UsageMin; u <= c.Range.UsageMax; ++u)
                push DetectedValue{c.UsagePage, u, c.LinkCollection, c.LogicalMin, c.LogicalMax, c.LogicalMax>c.LogicalMin, valueName(c.UsagePage,u)};
        } else {
            push DetectedValue{c.UsagePage, c.NotRange.Usage, c.LinkCollection, c.LogicalMin, c.LogicalMax, c.LogicalMax>c.LogicalMin, valueName(c.UsagePage,c.NotRange.Usage)};
        }
    }

    result.valid = true;
    return result;
}
```

**关键点**：
- `USHORT` 计数参数（非 `ULONG`），按 Windows SDK 定义
- `HIDP_BUTTON_CAPS`/`HIDP_VALUE_CAPS` 为 union（`Range`/`NotRange`），必须按 `IsRange` 分支访问
- Range 展开时 `u == 0` 视为结束哨兵，防止无限循环

---

## 使用流程

1. `backend/device_manager.cpp` → `indexDeviceCapabilities()` 对每个 collection 调用 `enumerate()`
2. `backend/auto_config.cpp` → `autoConfigure()` 遍历所有 target device 的 capabilities，**去重合并**（按 `usage+linkCollection` 对按钮，`usagePage+usage+linkCollection` 对轴）
3. 合并后的 `HidCapabilities` 传给 `adapter/config_mapper.cpp` → `autoDetect()` 生成 `MyHidConfig.buttons/axes`

---

## 测试要点

- 单 collection 设备：buttons/values 非空、valid=true
- 多 collection 复合设备：每个 collection 单独 enumerate，合并后去重正确
- 无 Input caps 设备：valid=false，buttons/values 为空
- Range 展开边界：UsageMin=1, UsageMax=7 → 生成 7 个 DetectedButton