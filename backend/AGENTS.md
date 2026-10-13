# backend 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `hid_input_backend.h/.cpp` | 公共 API 入口，薄封装层 |
| `device_manager.h/.cpp` | 目标设备跟踪、多集合管理、capability 索引 |
| `state_publisher.h/.cpp` | 用 mutex 保护最新状态快照 |
| `auto_config.h/.cpp` | 自动检测编排 |
| `backend.md` | 数据流、关键设计、接口、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`、`core/capabilities/hid_capabilities.h`、`platform/raw_input/*`、`adapter/my_hid_adapter.h`
- 被依赖：`cli/commands/select_cmd.cpp`、`cli/main.cpp`

## 分层原则

- **hid_input_backend**：对外接口，仅委托内部组件
- **device_manager**：设备生命周期、所有权、capability 索引
- **state_publisher**：对快照读写加锁；不负责同步 backend 的其他成员
- **auto_config**：编排 device_manager + adapter 完成自动检测

## 公共接口汇总

| 类/函数 | 用途 |
|---------|------|
| `HidInputBackend::start/stop` | 后端生命周期 |
| `HidInputBackend::tryGetLatest` | 主线程读取最新状态 |
| `HidInputBackend::setTargetVidPid` | 运行时切换设备 |
| `HidInputBackend::autoConfigure` | 选择设备时触发自动检测 |
| `DeviceManager::scanTargetDevices/tryAddTargetDevice` | 设备扫描与懒加载 |
| `DeviceManager::ownsButton/ownsAxis` | 多集合信号合并判定 |
| `StatePublisher::publishState/publishDisconnectedState` | 报告处理路径发布快照；回调实际线程受窗口线程归属问题影响 |
| `AutoConfig::configure` | 自动检测主流程 |

`configMutex` 不覆盖 Raw Input 回调对 backend 配置、adapter、DeviceManager 或 master state 的访问；CLI 线程上的设备切换与报告处理不能假定已同步。

## 修改指南

- 修改多集合合并策略 → `device_manager.cpp` `ownsButton/ownsAxis` + `backend.md`
- 变更状态发布机制（如改用 lock-free） → `state_publisher.cpp` + `backend.md`
- 新增自动检测步骤（如预设映射覆盖） → `auto_config.cpp` + `backend.md`
- 调整公共 API → `hid_input_backend.h/.cpp` + 所有调用方