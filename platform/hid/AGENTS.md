# platform/hid 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `hid_parser.h/.cpp` | HidP_GetUsages / HidP_GetUsageValue 封装 |
| `hid_usage_names.h/.cpp` | Usage Page/ID → 名称映射 |
| `hid.md` | 接口、映射表、使用流程、测试要点 |

## 依赖关系

- 依赖：Windows SDK `<windows.h>` `<hidsdi.h>`
- 被依赖：`adapter/button_parser.cpp`、`adapter/axis_parser.cpp`、`core/capabilities/hid_capabilities.cpp`、`adapter/config_mapper.cpp`

## 设计约束

1. **纯 API 封装**：无状态、无业务逻辑、无缓存
2. **错误处理**：返回 `bool` 表示 HID API 是否成功；`getUsageValue` 仅在成功时写入输出值
3. **报告输入**：报告以只读输入方式使用；因 Windows HID API 参数签名要求，调用处通过 `const_cast` 传递
4. `getActiveButtonUsages` 会在调用 HID API 前将输出向量重置为 64 个元素；失败时不保证保留调用前的向量内容

## 修改指南

- 新增 HidP_* 调用 → 在 `hid_parser.h/.cpp` 增加封装函数
- 扩展 Usage 名称映射 → 修改 `hid_usage_names.cpp` `genericDesktopName` 与 `hid.md` 映射表
- 变更报告类型（Input/Output/Feature） → 修改 `hid_parser` 函数签名添加 `HIDP_REPORT_TYPE` 参数