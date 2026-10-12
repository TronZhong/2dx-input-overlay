# core 模块文档索引

## 子模块

| 子模块 | AGENTS.md | 核心职责 |
|--------|-----------|----------|
| `types/` | [types/AGENTS.md](types/AGENTS.md) | 核心数据结构（仅动态映射）、计算规则 |
| `profiles/` | [profiles/AGENTS.md](profiles/AGENTS.md) | Profile 存储、JSON 序列化、Legacy 迁移 |
| `capabilities/` | [capabilities/AGENTS.md](capabilities/AGENTS.md) | HID 能力枚举（Button/Value Caps） |

## 分层原则

- **core/types** 最底层：纯数据结构，**零依赖**（除 STL/Windows 类型别名）
- **core/capabilities** 依赖 types：枚举原始 HID caps，输出 `HidCapabilities`
- **core/profiles** 依赖 types：持久化 `MyHidConfig`，含 Legacy 兼容迁移

上层模块（`adapter`、`backend`、`cli`）仅依赖 `core/types` 与 `core/profiles` 接口，**不直接依赖** `core/capabilities`（通过 `adapter/config_mapper` 中转）。

## 公共接口汇总

| 模块 | 关键类型/函数 | 用途 |
|------|---------------|------|
| `types` | `ButtonMapping`、`AxisMapping`、`MyHidConfig`、`MyHidState`、`HidOverlayState`、`DeviceInfo` | 全程贯穿的数据契约 |
| `profiles` | `DeviceProfileManager::loadProfile/saveProfile/listProfiles` | CLI 选择/保存设备配置 |
| `capabilities` | `CapabilityInspector::enumerate(preparsed)` | 后端自动检测入口 |

## 修改指南

- 修改 `types/hid_types.h` 结构体 → **必须**同步更新：
  - `types/hid_types.md`（字段表、计算规则）
  - `profiles/profile_serialization.cpp`（JSON 映射）
  - `adapter/config_mapper.cpp`（Caps→Config 转换）
  - `backend/state_publisher.cpp`（State→Overlay 发布）
  - `cli/formatters/*.cpp`（显示格式化）
- 新增 Profile 字段 → 更新 `profiles/` 两文件 + `profiles.md`
- 修改能力枚举逻辑 → 仅改 `capabilities/hid_capabilities.cpp` + `capabilities.md`