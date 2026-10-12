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
- `outUsages` 预分配 64 元素，成功后 `resize(usageLength)`
- 返回 `bool` 表示成功，失败不修改输出参数
- `const_cast` 用于兼容 Windows SDK 非 const 参数

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

## 测试要点

- `getActiveButtonUsages`：按下多键时返回所有 Usage；无按键时返回空向量
- `getUsageValue`：超出范围/不存在的 Usage 返回 false
- `valueName`：0x01 页已知 usage 返回语义名，其他返回十六进制
- 参数边界：`linkCollection` 为 0..255，`reportSize` 与 preparsed 匹配