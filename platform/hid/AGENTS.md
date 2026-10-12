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
2. **错误处理**：返回 `bool`，失败不修改输出参数
3. **只读**：不修改 preparsed data、不修改报告缓冲区

## 修改指南

- 新增 HidP_* 调用 → 在 `hid_parser.h/.cpp` 增加封装函数
- 扩展 Usage 名称映射 → 修改 `hid_usage_names.cpp` `genericDesktopName` 与 `hid.md` 映射表
- 变更报告类型（Input/Output/Feature） → 修改 `hid_parser` 函数签名添加 `HIDP_REPORT_TYPE` 参数