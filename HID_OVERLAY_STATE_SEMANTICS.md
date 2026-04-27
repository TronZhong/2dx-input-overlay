# HidOverlayState 输入语义说明（内部）

最后更新时间：2026-04-24

## 目的

本文定义渲染侧消费结构 `HidOverlayState` 的字段语义、来源与刷新行为，避免后端与 OBS Source 之间发生语义漂移。

结构定义位置：`hid_input_backend.h`
桥接映射位置：`obs-plugintemplate/src/hid-backend-bridge.cpp`

## 字段语义

| 字段 | 类型 | 来源 | 取值范围 | 刷新频率 | 说明 |
|---|---|---|---|---|---|
| `connected` | `bool` | Raw Input 设备状态 + 解析结果 | `true`/`false` | 每次发布快照 | 当前目标 VID/PID 设备是否在线并可解析 |
| `button01`..`button07` | `bool` | `HidP_GetUsages` 返回的当前按下 usage 列表 | `true`/`false` | 每帧输入报告 | 7 键按下状态；usage 映射由 `MyHidConfig` 决定 |
| `xNorm` | `float` | `HidP_GetUsageValue` 读取轴值后归一化 | `[0.0, 1.0]` | 每帧输入报告 | 轴原始值按 `xLogicalMin/xLogicalMax` 归一化并钳制 |
| `xDirection` | `int` | 当前帧轴增量与短时保持逻辑 | `-1`/`0`/`1` | 每帧输入报告 | `-1` 左、`1` 右、`0` 静止；会在 `xIdleTimeoutMs` 内短暂保持方向 |
| `tickMs` | `uint64_t` | Windows Tick（毫秒） | 非负整数 | 每次发布快照 | 快照发布时间戳，供渲染侧做新鲜度判断 |

## 计算规则（关键）

1. `button01..button07`
- 仅依赖 HID usage 列表，不做“按键历史”推断。
- 未出现在 usage 列表中的键视为未按下。

2. `xNorm`
- 归一化公式：

$$
xNorm = clamp\left(\frac{xRaw - xLogicalMin}{xLogicalMax - xLogicalMin}, 0, 1\right)
$$

- 若 `xLogicalMax <= xLogicalMin`，返回 `0.0`。

3. `xDirection`
- 对相邻两帧 `xRaw` 做差得到 `xDeltaRaw`。
- 针对常见 0..255 旋钮报告做环绕修正：
  - 若 `delta > 128`，则 `delta -= 256`
  - 若 `delta < -128`，则 `delta += 256`
- 方向判定：
  - `xDeltaRaw > 0` -> `1`
  - `xDeltaRaw < 0` -> `-1`
  - `xDeltaRaw == 0` -> 在 `xIdleTimeoutMs` 内保持上一方向，否则回落 `0`

## OBS 侧字段对齐

桥接层 `hid_backend_bridge_try_get_latest(...)` 必须保持以下一一映射：

- `connected` -> `connected`
- `button01..button07` -> `button_01_pressed..button_07_pressed`
- `xNorm` -> `x_norm`
- `xDirection` -> `x_direction`
- `tickMs` -> `tick_ms`

`obs-plugintemplate/src/input-overlay-source.c` 渲染逻辑仅消费上述快照字段，不直接调用 HID API。

## 兼容性约束

- 新增字段时，必须同步更新：
  - `hid_input_backend.h` 的结构定义
  - `obs-plugintemplate/src/hid-backend-bridge.h/.cpp` 的桥接结构与赋值
  - `obs-plugintemplate/src/input-overlay-source.c` 的消费逻辑
- 若仅改字段含义不改名字，必须先更新本文档并在 `DO_TODO_LIST.md` 交接记录注明。
