# platform/enumerator 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `device_enumerator.h/.cpp` | 系统 HID 设备枚举实现 |
| `enumerator.md` | 算法、关键点、使用流程、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`（`DeviceInfo`）、Windows SDK `<windows.h>` `<hidsdi.h>`
- 被依赖：`cli/commands/list_cmd.cpp`、`cli/commands/select_cmd.cpp`

## 设计约束

1. **纯枚举**：不缓存、不持久化、不做业务过滤
2. **错误宽容**：单设备查询失败不影响其他设备
3. **无状态**：静态方法，可并发调用

## 修改指南

- 修改 `DeviceInfo` 字段 → 同步更新 `core/types/hid_types.h` 与 `enumerator.md`
- 新增过滤条件（如仅 Joystick/Gamepad）→ 在 `enumerateAll` 内部添加，或由调用方过滤
- 变更设备路径编码 → 修改 `wideToUtf8` / `getRawInputDeviceNameW`