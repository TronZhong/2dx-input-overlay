# cli 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `main.cpp` | 入口、命令循环、状态轮询 |
| `commands/command.h` | 命令基类 |
| `commands/list_cmd.h/.cpp` | 列出设备 |
| `commands/select_cmd.h/.cpp` | 选择设备、Profile/自动检测 |
| `commands/print_cmd.h/.cpp` | 打印配置 |
| `commands/write_cmd.h/.cpp` | 保存 Profile |
| `commands/quit_cmd.h/.cpp` | 退出 |
| `formatters/device_formatter.h/.cpp` | 设备列表/集合格式化 |
| `formatters/config_formatter.h/.cpp` | 配置打印 |
| `formatters/caps_formatter.h/.cpp` | 能力表头 |
| `formatters/live_formatter.h/.cpp` | 实时状态行 |
| `cli.md` | 架构、命令分发、格式化器、测试要点 |

## 依赖关系

- 依赖：`core/types/hid_types.h`、`core/profiles/device_profile.h`、`backend/hid_input_backend.h`、`platform/enumerator/device_enumerator.h`、所有 formatters
- 被依赖：无（入口层）

## 分层原则

- **main.cpp**：组装 Context、命令映射、主循环（轮询+输入）
- **commands/**：每个命令单文件，仅通过 `CliContext` 交互，无全局状态
- **formatters/**：纯格式化函数，无副作用，便于测试/替换

## 公共接口汇总

| 命令 | 类 | 关键逻辑 |
|------|-----|----------|
| `l` | `ListCommand` | `DeviceEnumerator::enumerateAll` + `device_formatter::printDeviceList` |
| `s N` | `SelectCommand` | 索引校验 → 加载 Profile（只设置 backend VID/PID，未应用 Profile 映射）或调用 `autoConfigure` → 更新 `ctx.config` |
| `p` | `PrintCommand` | `config_formatter::printConfig` |
| `w [name]` | `WriteCommand` | `profileMgr.saveProfile` |
| `q` | `QuitCommand` | `ctx.running = false` |

## 修改指南

- 新增命令 → 继承 `Command`，实现 `execute`，在 `main.cpp` 注册到 map
- 修改输出格式 → 仅改对应 `formatters/*.cpp`，命令逻辑不变
- 变更交互流程（如添加确认提示） → 修改 `SelectCommand`/`WriteCommand`
- 实时输出策略 → `live_formatter::stateChanged` + `main.cpp` 轮询逻辑