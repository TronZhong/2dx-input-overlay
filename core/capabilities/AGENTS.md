# core/capabilities 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `hid_capabilities.h` | DetectedButton/DetectedValue/HidCapabilities 定义、CapabilityInspector 接口 |
| `hid_capabilities.cpp` | 枚举实现（HidP_GetButtonCaps/ValueCaps） |
| `capabilities.md` | 字段来源、枚举算法、使用流程、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`（无，仅 Windows SDK `<hidsdi.h>`）
- 被依赖：`backend/device_manager.cpp`、`backend/auto_config.cpp`、`adapter/config_mapper.cpp`

## 设计约束

1. **纯枚举**：不包含任何业务过滤、去重、映射逻辑
2. **原始数据**：保留 `linkCollection`，后续合并去重需要
3. **仅 Range 值作为轴**：`isRange==false` 的标量值不进入 `values`（或标记 `isRange=false` 供上层忽略）
4. **错误处理**：`preparsed==nullptr` 返回 `valid=false` 空结构；API 失败同理

## 修改指南

- 新增 Usage Page 名称映射 → 更新 `valueName()` 与 `capabilities.md` 表格
- 修改枚举范围（如改用 Feature/Output report） → 同步更新 `hid_capabilities.cpp` 与文档
- 结构体增删字段 → 检查 `config_mapper.cpp` 转换逻辑、`device_manager.cpp` 合并去重键