# adapter 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `my_hid_adapter.h/.cpp` | 公共接口、配置持有、轴追踪状态、委托解析器 |
| `button_parser.h/.cpp` | 按钮解析（GetUsages → dynamicButtons） |
| `axis_parser.h/.cpp` | 轴解析（GetUsageValue → 归一化/方向追踪） |
| `config_mapper.h/.cpp` | HidCapabilities → MyHidConfig.buttons/axes |
| `adapter.md` | 详细算法、接口、关键点、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`、`core/capabilities/hid_capabilities.h`、`platform/hid/*`
- 被依赖：`backend/auto_config.cpp`、`backend/device_manager.cpp`（通过 HidInputBackend）

## 分层原则

- **my_hid_adapter**：对外接口，持有配置与跨帧状态（`axisTracks_`）
- **button_parser/axis_parser**：无状态纯函数，单帧解析逻辑
- **config_mapper**：能力枚举到配置的一次性映射

## 公共接口汇总

| 函数 | 用途 |
|------|------|
| `MyHidAdapter::matches` | 后端设备过滤 |
| `MyHidAdapter::updateFromReport` | 输入线程每帧解析 |
| `MyHidAdapter::autoDetect` | 自动检测时生成动态映射 |
| `MyHidAdapter::config` | 读取当前配置（含动态映射） |
| `button_parser::parseButtons` | 按钮解析实现 |
| `axis_parser::parseAxes` | 轴解析实现 |
| `config_mapper::mapCapabilitiesToConfig` | 能力→配置转换 |

## 修改指南

- 修改归一化公式 → `axis_parser.cpp` + `my_hid_adapter.cpp` `normalizeToUnit` + `adapter.md`
- 变更方向追踪逻辑（超时、环绕阈值） → `axis_parser.cpp` + `my_hid_adapter.cpp` `trackAxisDirection` + `adapter.md`
- 新增按钮查询策略（如按 linkCollection 分组） → `button_parser.cpp` + `adapter.md`
- 调整能力映射规则（如过滤特定 Usage Page） → `config_mapper.cpp` + `adapter.md`
- 新增配置字段 → `core/types/hid_types.h` + `config_mapper.cpp` + `profile_serialization.cpp`