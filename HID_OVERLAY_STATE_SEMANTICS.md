# HidOverlayState 输入语义说明（内部）

最后更新时间：2026-09-20

## 目的

本文定义 `HidOverlayState` 快照结构的字段来源、语义与刷新行为。该结构由后端发布、供消费侧（当前 `main.cpp` 打印；未来独立窗口渲染）读取。OBS 插件方案已删除，本文不再描述 OBS 桥接映射。

结构定义位置：`hid_input_backend.h`
发布位置：`hid_input_backend.cpp`（`publishState`）

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

## 兼容性说明

- 当前 `HidOverlayState` 保留固定 7 按键 + 1 轴字段，供 `main.cpp` 消费打印。
- 自动检测生成的动态映射通过 `MyHidAdapter::autoDetect` 生成，解析时回填到 `MyHidState`，再经 `publishState` 映射为 `HidOverlayState`。
- 若未来改为动态字段（如变长按钮/轴数组），需同步更新 `hid_input_backend.cpp` 的 `publishState` 以及与本文档对应的消费语义。
