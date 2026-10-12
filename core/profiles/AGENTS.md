# core/profiles 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `device_profile.h/.cpp` | DeviceProfile 结构、DeviceProfileManager 接口与实现 |
| `profile_serialization.h/.cpp` | JSON (de)serialization、Legacy 字段迁移逻辑 |
| `profiles.md` | JSON 格式、迁移规则、接口说明、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`、`third_party/nlohmann/json.hpp`、`<filesystem>`
- 被依赖：`main.cpp`、`cli/commands/select_cmd.cpp`、`cli/commands/write_cmd.cpp`

## 设计约束

1. **仅新格式持久化**：`saveProfile` 写 VID/PID、`buttons[]`、`axes[]` 和名称，不写 legacy 字段
2. **按映射类别兼容**：按钮和轴分别优先读动态数组；某一类动态数组缺失或非数组时，才尝试从对应 legacy 字段重建
3. **错误行为按接口区分**：`loadProfile` 捕获读取/解析异常并记录日志；`listProfiles` 对单文件解析异常静默跳过；`saveProfile` 捕获异常，但不检查输出流最终状态；删除失败仅通过返回值报告
4. **目录相对工作目录**：默认 `profiles/`，可通过构造函数指定绝对路径

## 修改指南

- 新增 Profile 字段（如 `firmwareVersion`）→ 同步更新 `DeviceProfile`、`configToJson`/`configFromJson`、`profiles.md`
- 修改 JSON 字段默认值或迁移条件 → 更新 `profile_serialization.cpp` 与 `profiles.md`
- 变更存储路径/命名规则 → 修改 `profilePath()` 与 `profiles.md` 目录约定
- 增加并发访问保证 → 在 `DeviceProfileManager` 增加同步机制