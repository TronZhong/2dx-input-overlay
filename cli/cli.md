# CLI 模块

文件位置：`cli/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `main.cpp` | 入口、命令分发循环、状态轮询、Context 管理 |
| `commands/command.h` | 命令基类接口 |
| `commands/list_cmd.h/.cpp` | `l` - 列出设备 |
| `commands/select_cmd.h/.cpp` | `s N` - 选择设备、加载 profile 或自动检测 |
| `commands/print_cmd.h/.cpp` | `p` - 打印当前配置 |
| `commands/write_cmd.h/.cpp` | `w [name]` - 保存 profile |
| `commands/quit_cmd.h/.cpp` | `q` - 退出 |
| `formatters/device_formatter.h/.cpp` | 设备列表/集合格式化 |
| `formatters/config_formatter.h/.cpp` | 配置打印格式化 |
| `formatters/caps_formatter.h/.cpp` | 能力检测结果格式化 |
| `formatters/live_formatter.h/.cpp` | 实时状态行格式化（时间戳、按钮、轴） |

---

## CliContext

```cpp
struct CliContext {
    HidInputBackend& backend;
    DeviceProfileManager& profileMgr;
    std::vector<DeviceInfo> devices;
    MyHidConfig config;
    bool running = true;
};
```

贯穿整个 CLI 生命周期的共享状态。`ctx.config` 是 CLI 自己维护的副本；后端配置必须通过 backend API 设置或读取，两者不会自动同步。

---

## 命令分发

```cpp
// main.cpp 伪代码
std::map<char, std::unique_ptr<Command>> commands = {
    {'l', std::make_unique<ListCommand>()},
    {'s', std::make_unique<SelectCommand>()},
    {'p', std::make_unique<PrintCommand>()},
    {'w', std::make_unique<WriteCommand>()},
    {'q', std::make_unique<QuitCommand>()},
};

while (ctx.running) {
    // 1. 轮询后端状态
    HidOverlayState state;
    if (ctx.backend.tryGetLatest(state)) {
        if (!_kbhit() && (!hasLast || live_formatter::stateChanged(state, last))) {
            std::cout << live_formatter::formatLiveLine(ctx.config, state) << "\n";
            std::cout << "Enter command> ";
        }
        last = state;
        hasLast = true;
    }

    // 2. 非阻塞命令输入
    if (_kbhit()) {
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        char cmd = line[0];
        std::string args = line.size() > 1 ? line.substr(1) : "";
        if (auto it = commands.find(cmd); it != commands.end()) {
            it->second->execute(ctx, args);
        } else {
            std::cout << "Unknown command. Use: l, s N, p, w [name], q\n";
        }
        std::cout << "Enter command> ";
    }
    Sleep(5);
}
```

---

## 格式化器

### device_formatter
- `printDeviceList`：`l` 命令输出
- `printDeviceCollections`：`s N` 后显示该 VID/PID 下所有 collection

### config_formatter
- `printConfig`：`p` 命令输出，显示动态 buttons/axes 完整信息

### caps_formatter
- `printCaps`：`s N` 自动检测后显示 `HidCapabilities` 表头

### live_formatter
- `fmtTimestamp`：使用 `GetLocalTime()` 生成 `[HH:MM:SS.mmm]`；不使用状态快照的 `tickMs`
- `stateChanged`：语义变化检测（connected、buttons、axesNorm/Raw/Dir）
- `formatLiveLine`：实时行格式化
  - 按钮：`Btn: [0]1(Btn 1) [1]0(Btn 2)...`
  - 轴：`Axis: [0]=0.50(128)^(X) [1]=0.00(0)-(Y)`

---

## 交互命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `l` | - | 列出所有 HID 设备 |
| `s N` | 设备索引 | 选择设备，优先加载 profile，否则自动检测；自动检测失败时仅切换 VID/PID 并使用现有/空映射 |
| `p` | - | 打印当前动态映射配置 |
| `w [name]` | 可选名称 | 保存当前配置为 profile |
| `q` | - | 退出程序 |

---

## 测试要点

- 命令分发正确，参数解析健壮（非数字索引、越界）
- `s N` 后自动刷新设备列表显示 collections
- Profile 加载/保存循环：当前 Profile 映射尚未接入 backend；完成映射应用后再验证 save → restart → select
- 实时输出：状态变化才刷新、输入时暂停输出避免交错
- 无动态映射时优雅降级显示

## 启动时 Profile 的当前行为

`main()` 在创建 backend 时使用默认 `MyHidConfig`，之后会尝试读取该默认 VID/PID 的 Profile 并仅更新 `ctx.config`。启动 backend 前没有调用 `setTargetVidPid()`，也没有把 Profile 配置传回 backend；因此即使显示“Loaded profile”，后端仍使用构造时的默认配置。通过交互命令 `s N` 加载 Profile 时，则会调用 `setTargetVidPid()`，但该方法只设置 VID/PID，不会将 Profile 的按钮/轴映射写入 backend。此处为当前实现行为，Profile 映射的实际应用尚未接通。