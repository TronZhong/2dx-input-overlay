# core/types 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `hid_types.h` | 核心数据结构定义（仅动态映射，无 legacy） |
| `hid_types.md` | 类型字段语义、计算规则、迁移说明 |
| `device_info.md` | DeviceInfo 字段来源与用途 |

## 依赖关系

- **无外部依赖**：仅 `<cstdint>`、`<vector>`、`<string>`、`<windows.h>`（仅为 `HANDLE`/`USAGE` 等类型别名）
- 被依赖方：`core/profiles`、`core/capabilities`、`adapter`、`backend`、`cli`

## 设计约束

1. **纯数据结构**：无方法、无平台逻辑、无构造/析构副作用
2. **仅动态映射**：`MyHidConfig`/`MyHidState`/`HidOverlayState` 均不包含 legacy 固定字段
3. **不变量**：
   - `config.buttons.size() == state.dynamicButtons.size() == overlay.buttons.size()`
   - `config.axes.size() == state.dynamicAxes*.size() == overlay.axes*.size()`
4. **Profile 兼容**：旧 JSON 由 `profile_serialization` 读取时自动转换为新结构

## 修改指南

- 新增字段 → 同步更新 `hid_types.md` 表格与计算规则
- 修改字段语义 → 同步更新 `adapter/axis_parser.cpp` 归一化/方向逻辑、`backend/state_publisher.cpp` 发布逻辑
- 结构体增删 → 检查 `profile_serialization` (de)serialization、CLI formatters