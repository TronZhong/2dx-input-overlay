# core/capabilities 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `hid_capabilities.h` | DetectedButton/DetectedValue/HidCapabilities 定义、CapabilityInspector 接口 |
| `hid_capabilities.cpp` | 枚举实现（HidP_GetButtonCaps/ValueCaps） |
| `capabilities.md` | 字段来源、枚举算法、使用流程、测试要点 |

## 依赖关系

- 依赖：Windows SDK `<hidsdi.h>`；输出结构声明在本模块头文件，并不依赖 `core/types/hid_types.h`
- 被依赖：`backend/device_manager.cpp`、`backend/auto_config.cpp`、`adapter/config_mapper.cpp`

## 设计约束

1. **纯枚举**：不包含任何业务过滤、去重、映射逻辑
2. **原始数据**：保留 `linkCollection`，后续合并去重需要
3. **记录全部值**：Range 和 NotRange 都进入 `values`；`isRange` 根据 `LogicalMax > LogicalMin` 设置，后续配置映射仅将 `isRange==true` 的值作为轴
4. **错误处理**：`preparsed==nullptr` 返回默认空结构（`valid=false`）；非空时函数结束会设 `valid=true`，即使 HidP API 调用失败或没有能力项

## 修改指南

- 新增 Usage Page 名称映射 → 更新 `valueName()` 与 `capabilities.md` 表格
- 修改枚举范围（如改用 Feature/Output report） → 同步更新 `hid_capabilities.cpp` 与文档
- 结构体增删字段 → 检查 `config_mapper.cpp` 转换逻辑、`device_manager.cpp` 合并去重键