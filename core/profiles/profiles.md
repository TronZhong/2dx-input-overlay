# Profile 系统

文件位置：`core/profiles/device_profile.h/.cpp`、`core/profiles/profile_serialization.h/.cpp`

## 概览

- **存储**：`profiles/<VID>_<PID>.json`（如 `034C_0368.json`）
- **内容**：完整 `MyHidConfig`（`vid`/`pid` + `buttons[]` + `axes[]`）+ 可选 `name`
- **加载时机**：`cli select` 命令（`s N`）时，优先加载 profile，无则触发自动检测
- **保存时机**：`cli write` 命令（`w [name]`）将当前动态映射持久化

---

## JSON 格式（新格式）

```json
{
  "vid": 844,
  "pid": 872,
  "name": "PhoenixWan+",
  "buttons": [
    {"usagePage": 9, "usage": 1, "linkCollection": 0, "name": "Btn 1", "index": 0},
    {"usagePage": 9, "usage": 2, "linkCollection": 0, "name": "Btn 2", "index": 1}
  ],
  "axes": [
    {"usagePage": 1, "usage": 48, "linkCollection": 0, "logicalMin": 0, "logicalMax": 255, "name": "X", "index": 0},
    {"usagePage": 1, "usage": 49, "linkCollection": 0, "logicalMin": 0, "logicalMax": 255, "name": "Y", "index": 1}
  ]
}
```

字段对应 `core/types/hid_types.h` 的 `ButtonMapping`/`AxisMapping`。

---

## Legacy 兼容（迁移逻辑）

旧 Profile 仅包含固定字段：

```json
{
  "vid": 844,
  "pid": 872,
  "buttonUsagePage": 9,
  "button_01Usage": 1,
  "button_02Usage": 2,
  ...
  "axisUsagePage": 1,
  "xUsage": 48,
  "xLogicalMin": 0,
  "xLogicalMax": 255,
  ...
}
```

**迁移规则**（`profile_serialization::configFromJson`）：

1. 若 JSON 含 `buttons[]`/`axes[]` → 直接反序列化为动态映射
2. 否则读取 legacy 字段：
   - `buttonUsagePage` + `button_01Usage`..`button_07Usage` + `buttonLinkCollection` → 构造 `buttons[0..6]`
   - `axisUsagePage` + `xUsage` + `axisLinkCollection` + `xLogicalMin/Max` → 构造 `axes[0]` (X)
   - `enableYAxis==true` → 额外构造 `axes[1]` (Y, usage=0x31)

**保存时仅写新格式**：`configToJson` 只输出 `buttons[]`/`axes[]`，不写 legacy 字段。旧文件加载后再次保存即自动完成迁移。

---

## DeviceProfileManager 接口

| 方法 | 说明 |
|------|------|
| `loadProfile(vid, pid)` | 读取对应 JSON，返回 `optional<DeviceProfile>` |
| `saveProfile(config, name)` | 写入/覆盖 `<vid>_<pid>.json`，`name` 为空时默认 `Device_XXXX_XXXX` |
| `listProfiles()` | 扫描 `profiles/` 目录，返回所有有效 profile |
| `deleteProfile(vid, pid)` | 删除对应 JSON 文件 |
| `getProfilesDir()` | 返回目录路径（默认 `profiles/`，相对工作目录） |

---

## 目录约定

- 开发时：`profiles/` 在源码根目录（`run_build.cmd` 工作目录）
- 部署时：可传入绝对路径构造 `DeviceProfileManager`
- 目录不存在时自动创建（`ensureDirExists`）

---

## 错误处理

- 文件读写失败 → 打印 `stderr`，返回 `false`/`nullopt`，**不抛异常**
- JSON 解析失败 → 同理，跳过该文件（`listProfiles`）或返回空
- 并发访问：当前单线程 CLI 无竞争；若后端多线程读取，需外部加锁

---

## 测试要点

1. 新格式读写循环：save → load → 配置完全一致
2. 旧格式加载 → `buttons[]`/`axes[]` 正确重建
3. 旧格式加载后再次保存 → 文件变为新格式
4. 不存在的 VID/PID → `loadProfile` 返回 `nullopt`
5. `listProfiles` 跳过损坏 JSON、非 `.json` 文件