# Adapter 模块

文件位置：`adapter/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `my_hid_adapter.h/.cpp` | 公共接口：matches/updateFromReport/autoDetect/config()，持有配置与轴追踪状态 |
| `button_parser.h/.cpp` | 按钮解析：获取活跃 Usage 列表，按动态映射判定按下状态 |
| `axis_parser.h/.cpp` | 轴解析：读取原始值、归一化、方向追踪 |
| `config_mapper.h/.cpp` | HidCapabilities → MyHidConfig.buttons/axes 映射 |

---

## MyHidAdapter

### 状态
- `cfg_`：当前配置（含动态 `buttons`/`axes`）
- `axisTracks_`：每轴方向追踪状态（`hasPrev`/`prevRaw`/`lastDir`/`lastMoveTickMs`），`mutable` 允许 `const updateFromReport` 修改

### 接口
```cpp
bool matches(uint16_t vid, uint16_t pid) const;  // VID/PID 匹配
bool updateFromReport(PHIDP_PREPARSED_DATA, const uint8_t*, size_t, MyHidState&) const;  // 核心解析
bool autoDetect(const HidCapabilities& caps);  // 能力→配置映射
const MyHidConfig& config() const;  // 读取配置
```

---

## ButtonParser

### parseButtons
```cpp
void parseButtons(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState);
```

### 算法
1. 若 `config.buttons` 为空 → 清空 `outState.dynamicButtons`，返回
2. 取 `config.buttons[0]` 的 `usagePage`/`linkCollection`，调 `hid_parser::getActiveButtonUsages` 获取当前按下的所有 Usage；当前不检查此调用的 bool 返回值
3. `outState.dynamicButtons` 赋值为 `config.buttons.size()` 个 `false`
4. 遍历 `config.buttons`，对每个 mapping 在 `activeUsages` 中查找 `usage`，找到则 `pressed=true`
5. 写入 `outState.dynamicButtons[i] = pressed`

### 关键点
- 所有按钮共用第一项映射的 `usagePage`/`linkCollection` 查询；合并多个 collection 后若映射项来自不同集合，其他集合的按钮不会按其各自 collection 查询
- 匹配只比较 Usage，不比较映射项的 `usagePage`/`linkCollection`
- 无 legacy 回退路径（新架构仅动态映射）

---

## AxisParser

### parseAxes
```cpp
void parseAxes(
    PHIDP_PREPARSED_DATA preparsed,
    const uint8_t* rawData,
    size_t rawSize,
    const MyHidConfig& config,
    MyHidState& outState,
    std::vector<MyHidAdapter::AxisTrack>& axisTracks);
```

### 算法（每轴独立）
1. 若 `config.axes` 为空 → 清空三个动态向量，返回
2. 分配 `dynamicAxesNorm/Raw/Dir` 为 `config.axes.size()`
3. `now = currentTickMs()`
4. 遍历 `config.axes`：
   - `hid_parser::getUsageValue` 读取原始值；失败时该轴维持本帧预分配的 raw/norm/dir 零值并跳过后续计算
   - 归一化：`(raw - logicalMin) / (logicalMax - logicalMin)` clamp `[0,1]` → `dynamicAxesNorm[i]`
   - 方向追踪（复用 `axisTracks[i]`）：
     - `delta = raw - prevRaw`，环绕修正 `>128 → -256` / `<-128 → +256`
     - `delta>0` → `lastDir=1, lastMoveTickMs=now, dir=1`
     - `delta<0` → `lastDir=-1, lastMoveTickMs=now, dir=-1`
     - `delta==0` → 若 `lastDir!=0` 且 `now-lastMoveTickMs < 99ms` 保持 `lastDir`，否则 `lastDir=0, dir=0`
   - `dynamicAxesDir[i] = dir`
   - `axisTracks[i]` 更新 `prevRaw`/`hasPrev`/`lastDir`/`lastMoveTickMs`

### 关键点
- 每轴独立追踪，互不干扰
- `axisTracks` 为 `mutable` 成员，跨帧持久化
- 环绕修正对所有轴固定使用 ±128/256，不根据实际报告位宽或逻辑范围调整；仅适用于相应 8-bit 环绕语义
- 空闲超时 `IDLE_TIMEOUT_MS=99ms`（约 6 帧 @60Hz）

---

## ConfigMapper

### mapCapabilitiesToConfig
```cpp
bool mapCapabilitiesToConfig(const HidCapabilities& caps, MyHidConfig& config);
```

### 算法
1. `config.buttons.clear()`，`config.axes.clear()`
2. 遍历 `caps.buttons`：
   - `ButtonMapping{usagePage=0x09, usage=b.usage, linkCollection=b.linkCollection, name=b.name, index=i}`
3. 遍历 `caps.values`，**仅 `isRange==true`**：
   - `AxisMapping{usagePage=v.usagePage, usage=v.usage, linkCollection=v.linkCollection, logicalMin=v.logicalMin, logicalMax=v.logicalMax, name=v.name, index=axisIndex++}`
4. 返回 `!buttons.empty() || !axes.empty()`

### 关键点
- 按钮固定 `usagePage=0x09`（Button Page），`usage` 来自 caps
- 轴仅取 `isRange` 值，非范围标量忽略
- `index` 连续分配，与 `dynamicButtons/dynamicAxes*` 下标对应
- `name` 优先用 caps 自带，回退生成

---

## 使用流程

```
autoConfigure (backend)
    │
    ├─▶ CapabilityInspector::enumerate() ──▶ HidCapabilities
    │
    ├─▶ ConfigMapper::mapCapabilitiesToConfig() ──▶ MyHidConfig.buttons/axes
    │
    ▼
updateFromReport (每帧)
    │
    ├─▶ ButtonParser::parseButtons() ──▶ dynamicButtons[]
    │
    └─▶ AxisParser::parseAxes() ──▶ dynamicAxesNorm/Raw/Dir[]
```

---

## 测试要点

- 无动态映射时：向量为空，不崩溃
- 多按钮同时按下：`dynamicButtons` 全部正确
- 轴环绕：255→0 视为 +1，0→255 视为 -1
- 方向保持：停止移动后 99ms 内保持方向
- 多轴独立：X/Y/Z 方向互不干扰
- ConfigMapper：非 Range 值不进入 axes