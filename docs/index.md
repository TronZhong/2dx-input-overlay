# 文档索引

本文档按读者目标组织仓库现有说明。项目简介和快速运行入口见根目录 [README](../README.md)。

## 按目标阅读

### 使用程序

- [README：快速开始与交互命令](../README.md#快速开始)
- [Profile 系统](../core/profiles/profiles.md)：映射配置的保存、加载和旧格式迁移

### 构建与开发

- [构建与开发指南](build.md)：环境要求、构建命令和开发流程
- [待办与交接记录](../DO_TODO_LIST.md)：当前已完成工作和后续事项（项目内部记录）

### 理解设计

- [架构设计](architecture.md)：模块分层、依赖关系、线程模型和扩展点
- [数据流](data_flow.md)：从设备选择到报告解析、状态发布的端到端流程

### 查阅模块细节

| 模块 | 说明 |
|------|------|
| `core/types/` | [HID 核心类型](../core/types/hid_types.md)、[DeviceInfo](../core/types/device_info.md) |
| `core/capabilities/` | [HID 能力枚举](../core/capabilities/capabilities.md) |
| `core/profiles/` | [Profile 系统](../core/profiles/profiles.md) |
| `platform/enumerator/` | [设备枚举](../platform/enumerator/enumerator.md) |
| `platform/hid/` | [HID 解析](../platform/hid/hid.md) |
| `platform/raw_input/` | [Raw Input](../platform/raw_input/raw_input.md) |
| `backend/` | [Backend 编排](../backend/backend.md) |
| `adapter/` | [Report 解析与配置映射](../adapter/adapter.md) |
| `cli/` | [命令行交互](../cli/cli.md) |

各目录中的 `AGENTS.md` 是面向代码修改代理的局部开发约束，不是用户使用手册。
