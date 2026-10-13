# platform 模块文档索引

## 子模块

| 子模块 | AGENTS.md | 核心职责 |
|--------|-----------|----------|
| `enumerator/` | [enumerator/AGENTS.md](enumerator/AGENTS.md) | 系统 HID 设备枚举 |
| `raw_input/` | [raw_input/AGENTS.md](raw_input/AGENTS.md) | Raw Input 窗口、注册、解析 |
| `hid/` | [hid/AGENTS.md](hid/AGENTS.md) | HidP_* API 封装、Usage 名称映射 |

## 分层原则

- **platform/enumerator** 最上层：仅枚举设备列表，不涉及输入接收
- **platform/raw_input** 中层：窗口消息循环、输入接收、预解析数据获取
- **platform/hid** 最底层：具体报告解析 API 封装

上层模块（`backend`）组合使用三者：
1. `enumerator` 获取设备列表 → 用户选择 VID/PID
2. `raw_input` 当前在调用线程创建窗口、注册通配，再于 worker 运行消息循环；线程归属不一致，输入回调处理尚未验证可靠
3. `hid` 在回调中解析按钮/轴值

## 公共接口汇总

| 模块 | 关键类/函数 | 用途 |
|------|-------------|------|
| `enumerator` | `DeviceEnumerator::enumerateAll()` | CLI list、select 设备来源 |
| `raw_input` | `RawInputWindow`、`RawInputRegistry::registerAll`、`RawInputParser` | 后端 Raw Input 接收基础；窗口与消息泵线程需保持一致 |
| `hid` | `hid_parser::getActiveButtonUsages`、`getUsageValue`、`hid_usage_names::valueName` | 适配器解析、能力枚举命名 |

## 修改指南

- 新增设备枚举字段 → 修改 `enumerator/device_enumerator.cpp` + `core/types/hid_types.h` + `enumerator/enumerator.md`
- 变更 Raw Input 注册策略 → 修改 `raw_input/raw_input_registry.cpp` + `raw_input.md`
- 增加 HID 解析 API → 修改 `hid/hid_parser.h/.cpp` + `hid.md`
- 扩展 Usage 名称 → 修改 `hid/hid_usage_names.cpp` + `hid.md`