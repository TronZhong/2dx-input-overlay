# core/profiles 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `device_profile.h/.cpp` | DeviceProfile 结构、DeviceProfileManager 接口与实现 |
| `profile_serialization.h/.cpp` | JSON (de)serialization、Legacy 字段迁移逻辑 |
| `profiles.md` | JSON 格式、迁移规则、接口说明、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`、`third_party/nlohmann/json.hpp`、`<filesystem>`
- 被依赖：`cli/commands/select_cmd.cpp`、`cli/commands/write_cmd.cpp`、`backend/auto_config.cpp`

## 设计约束

1. **仅新格式持久化**：`saveProfile` 仅写 `buttons[]`/`axes[]`，不写 legacy 字段
2. **加载时双轨兼容**：优先读动态映射；无则从 legacy 重建
3. **无异常抛出**：所有 I/O 错误转为 `bool`/`optional` + `stderr` 日志
4. **目录相对工作目录**：默认 `profiles/`，可通过构造函数指定绝对路径

## 修改指南

- 新增 Profile 字段（如 `firmwareVersion`）→ 同步更新 `DeviceProfile`、`configToJson`/`configFromJson`、`profiles.md`
- 修改迁移逻辑 → 仅改 `profile_serialization.cpp`，保持 `device_profile.cpp` 不变
- 变更存储路径/命名规则 → 修改 `profilePath()` 与 `profiles.md` 目录约定
- 增加并发支持 → 在 `DeviceProfileManager` 增加 mutex，或调用方外部加锁