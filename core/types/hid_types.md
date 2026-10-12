# 核心类型定义

文件位置：`core/types/hid_types.h`

## 设计原则

- **仅动态映射**：移除所有 legacy 固定字段（7按键+1轴），仅保留 `vector<ButtonMapping>` 与 `vector<AxisMapping>`
- **平台无关**：不包含任何 Windows 特定逻辑，仅数据结构
- **语义自述**：字段命名即文档，配合本文档说明计算规则

---

## ButtonMapping

| 字段 | 类型 | 含义 |
|------|------|------|
| `usagePage` | `USAGE` | HID Usage Page（通常 0x09 Button） |
| `usage` | `USAGE` | HID Usage ID（如 0x01, 0x02...） |
| `linkCollection` | `ULONG` | 所属 Link Collection |
| `name` | `std::string` | 人类可读标签（如 "Btn 1", "Trigger"） |
| `index` | `size_t` | 在动态数组中的下标 |

**来源**：`CapabilityInspector::enumerate()` 从 `HIDP_BUTTON_CAPS` 生成，再经 `config_mapper` 转为 `ButtonMapping`。

---

## AxisMapping

| 字段 | 类型 | 含义 |
|------|------|------|
| `usagePage` | `USAGE` | HID Usage Page（如 0x01 Generic Desktop） |
| `usage` | `USAGE` | HID Usage ID（如 0x30 X, 0x31 Y） |
| `linkCollection` | `ULONG` | 所属 Link Collection |
| `logicalMin` | `LONG` | 逻辑最小值（来自 `HIDP_VALUE_CAPS.LogicalMin`） |
| `logicalMax` | `LONG` | 逻辑最大值（来自 `HIDP_VALUE_CAPS.LogicalMax`） |
| `name` | `std::string` | 人类可读标签（如 "X", "Y", "Wheel"） |
| `index` | `size_t` | 在动态数组中的下标 |

**来源**：`CapabilityInspector::enumerate()` 从 `HIDP_VALUE_CAPS`（仅 `IsRange==true`）生成，再经 `config_mapper` 转为 `AxisMapping`。

---

## MyHidConfig

设备匹配与映射配置的完整描述。

| 字段 | 类型 | 含义 |
|------|------|------|
| `vid` | `uint16_t` | Vendor ID |
| `pid` | `uint16_t` | Product ID |
| `buttons` | `vector<ButtonMapping>` | 所有按钮映射（按检测顺序，index 连续） |
| `axes` | `vector<AxisMapping>` | 所有轴映射（按检测顺序，index 连续） |

**匹配规则**：`matches(v, p)` 仅比对 VID/PID。

**持久化**：经 `profile_serialization` 序列化为 JSON（`profiles/<vid>_<pid>.json`），包含完整 buttons/axes 数组。

---

## MyHidState

解析器内部状态，`updateFromReport` 逐帧填充。

| 字段 | 类型 | 含义 |
|------|------|------|
| `connected` | `bool` | 当前帧是否成功解析 |
| `dynamicButtons` | `vector<bool>` | 与 `config.buttons` 等长，按钮按下状态 |
| `dynamicAxesNorm` | `vector<float>` | 与 `config.axes` 等长，归一化 `[0,1]` |
| `dynamicAxesRaw` | `vector<LONG>` | 与 `config.axes` 等长，原始值 |
| `dynamicAxesDir` | `vector<int>` | 与 `config.axes` 等长，方向 `-1/0/1` |

**不变量**：四个向量 `size()` 始终分别等于 `config.buttons.size()` 与 `config.axes.size()`。映射尺寸变化时由后端重分配。

---

## HidOverlayState

后端发布给消费侧（CLI/未来 GUI）的快照。

| 字段 | 类型 | 含义 |
|------|------|------|
| `connected` | `bool` | 设备在线且可解析 |
| `tickMs` | `uint64_t` | 发布时间戳（`GetTickCount64()`） |
| `buttons` | `vector<bool>` | `MyHidState.dynamicButtons` 副本 |
| `axesNorm` | `vector<float>` | `MyHidState.dynamicAxesNorm` 副本 |
| `axesRaw` | `vector<LONG>` | `MyHidState.dynamicAxesRaw` 副本 |
| `axesDir` | `vector<int>` | `MyHidState.dynamicAxesDir` 副本 |

**线程安全**：`state_publisher` 用 mutex 保护单生产者/单消费者。

---

## DeviceInfo

系统枚举返回的设备摘要。

| 字段 | 类型 | 含义 |
|------|------|------|
| `handle` | `HANDLE` | Raw Input 设备句柄 |
| `vid`/`pid` | `uint16_t` | 供应商/产品 ID |
| `name` | `wstring` | 设备路径字符串 |
| `usagePage`/`usage` | `USAGE` | Top-level collection Usage |

---

## 计算规则（规范）

### 归一化

```
norm = clamp((raw - logicalMin) / (logicalMax - logicalMin), 0.0, 1.0)
```
若 `logicalMax <= logicalMin` 则 `norm = 0.0`。

### 方向追踪（每轴独立）

状态机 `AxisTrack`：
- `hasPrev`：是否有前一帧
- `prevRaw`：前一帧原始值
- `lastDir`：上一次非零方向 `-1/1`
- `lastMoveTickMs`：上次移动时间

每帧：
```
delta = raw - prevRaw
// 环绕修正（假设 8-bit 报告 0..255）
if (delta > 128) delta -= 256;
else if (delta < -128) delta += 256;

if (delta > 0) { lastDir = 1; lastMoveTickMs = now; return 1; }
if (delta < 0) { lastDir = -1; lastMoveTickMs = now; return -1; }
// delta == 0
if (lastDir != 0 && (now - lastMoveTickMs) < IDLE_TIMEOUT_MS) return lastDir;
lastDir = 0; return 0;
```
`IDLE_TIMEOUT_MS` 由 `config` 指定（当前硬编码 99ms，约 6 帧 @60Hz）。

---

## 迁移说明（Legacy 移除）

| 旧字段 | 新对应 |
|--------|--------|
| `buttonUsagePage` | `buttons[0].usagePage` |
| `button_01Usage`..`button_07Usage` | `buttons[i].usage` |
| `buttonLinkCollection` | `buttons[i].linkCollection` |
| `axisUsagePage` | `axes[0].usagePage` |
| `xUsage` | `axes[0].usage` |
| `axisLinkCollection` | `axes[0].linkCollection` |
| `xLogicalMin/Max` | `axes[0].logicalMin/Max` |
| `xIdleTimeoutMs` | `AxisTrack` 内部常量（可后续外露） |
| `button_01Pressed`..`button_07Pressed` | `dynamicButtons[0..6]` |
| `xNorm/xRaw/xDeltaRaw/xDirection` | `dynamicAxesNorm/Raw/Dir[0]` |

旧 Profile JSON 仍可加载：`profile_serialization` 读取旧字段时自动构造等价的 `buttons`/`axes` 数组（见 `core/profiles/profile_serialization.cpp`）。