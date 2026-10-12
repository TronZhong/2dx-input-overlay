# HID 解析平台层

文件位置：`platform/hid/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `hid_parser.h/.cpp` | `HidP_GetUsages` / `HidP_GetUsageValue` 封装 |
| `hid_usage_names.h/.cpp` | Usage Page/ID → 人类可读名称映射 |

---

## hid_parser

### 接口

```cpp
namespace hid_parser {

// 获取报告中当前按下的所有按钮 Usage
bool getActiveButtonUsages(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    const uint8_t* report,
    ULONG reportSize,
    std::vector<USAGE>& outUsages);

// 获取单个轴/标量的原始值
bool getUsageValue(
    PHIDP_PREPARSED_DATA preparsed,
    USAGE usagePage,
    USHORT linkCollection,
    USAGE usage,
    const uint8_t* report,
    ULONG reportSize,
    LONG& outValue);

}
```

### 关键点
- 仅封装 `HidP_GetUsages` / `HidP_GetUsageValue`，参数透传
- `getActiveButtonUsages` 调用前将 `outUsages` 赋为 64 个零元素；HID API 成功后按返回的 `usageLength` 调整大小
- 返回 `bool` 表示 HID API 是否返回 `HIDP_STATUS_SUCCESS`
- `getUsageValue` 使用局部变量接收结果，仅在成功时写入 `outValue`
- HID API 的报告参数不是 const 指针；包装器通过 `const_cast` 适配签名，报告在此处作为解析输入使用

---

## hid_usage_names

### 接口

```cpp
namespace hid_usage_names {
const char* genericDesktopName(USAGE usage);  // 0x01 页语义名，无则 nullptr
std::string valueName(USAGE usagePage, USAGE usage); // 通用：0x01 页用语义名，否则十六进制
}
```

### 映射表（Generic Desktop Page 0x01）

| Usage | 名称 |
|-------|------|
| 0x30 | X |
| 0x31 | Y |
| 0x32 | Z |
| 0x33 | Rx |
| 0x34 | Ry |
| 0x35 | Rz |
| 0x36 | Slider |
| 0x37 | Dial |
| 0x38 | Wheel |
| 0x39 | Hat |

其他 Usage Page 或未列入表者 → `"0x" + hex(usage)`。

---

## 使用流程

1. `adapter/button_parser.cpp` 调 `hid_parser::getActiveButtonUsages`
2. `adapter/axis_parser.cpp` 调 `hid_parser::getUsageValue`
3. `core/capabilities/hid_capabilities.cpp` 枚举时调 `hid_usage_names::valueName` 生成 `DetectedValue.name`
4. `adapter/config_mapper.cpp` 将 `DetectedValue.name` 透传为 `AxisMapping.name`

---

## 建议验证场景

- `getActiveButtonUsages`：按下多键时返回所有 Usage；无按键时返回空向量
- `getUsageValue`：HID API 返回非成功状态时包装函数返回 `false`
- `valueName`：0x01 页已知 usage 返回语义名，其他返回十六进制
- 参数边界：`linkCollection` 使用 `USHORT` 类型，`reportSize` 与报告缓冲区和 preparsed data 匹配