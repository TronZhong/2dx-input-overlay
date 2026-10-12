# Profile 系统

文件位置：`core/profiles/`

Profile 将设备 VID/PID、按钮和轴映射保存为 JSON，供程序再次选择同一设备时恢复配置。

## 文件与调用流程

- 默认目录：进程当前工作目录下的 `profiles/`；构造 `DeviceProfileManager` 时可传入其他路径。
- 文件名：`%04X_%04X.json`，VID/PID 为大写十六进制，不足四位补零，例如 `034C_0368.json`。
- `s N`：[`SelectCommand`](../../cli/commands/select_cmd.cpp) 按所选设备的 VID/PID 尝试加载 profile；加载不到时才调用后端自动检测。
- `w [name]`：[`WriteCommand`](../../cli/commands/write_cmd.cpp) 保存当前配置。CLI 未提供名称时会生成 `Device_VID_PID`；`DeviceProfileManager::saveProfile` 本身不会生成该默认名称。

构造 `DeviceProfileManager` 时会尝试创建目录；保存前也会再次尝试。当前实现忽略目录创建时的 `error_code`。

## 当前 JSON 格式

`configToJson` 生成配置字段；`saveProfile` 额外写入 `name`。数值字段以 JSON 数字保存。

```json
{
  "vid": 844,
  "pid": 872,
  "buttons": [
    {
      "usagePage": 9,
      "usage": 1,
      "linkCollection": 0,
      "name": "Btn 1",
      "index": 0
    }
  ],
  "axes": [
    {
      "usagePage": 1,
      "usage": 48,
      "linkCollection": 0,
      "logicalMin": 0,
      "logicalMax": 255,
      "name": "X",
      "index": 0
    }
  ],
  "name": "PhoenixWan+"
}
```

字段含义见 [核心 HID 类型](../types/hid_types.md)。`name` 在保存时总会写入；若直接调用 `saveProfile(config)` 而未传名称，写入的值为空字符串。读取时若 JSON 缺少 `name`，名称默认为空字符串。

### 动态字段默认值

动态对象缺少字段时，反序列化器使用以下默认值：

| 对象 | 字段 | 默认值 |
|------|------|--------|
| `ButtonMapping` | `usagePage`、`usage`、`linkCollection`、`name`、`index` | `0x09`、`0`、`0`、空字符串、`0` |
| `AxisMapping` | `usagePage`、`usage`、`linkCollection`、`logicalMin`、`logicalMax`、`name`、`index` | `0x01`、`0x30`、`0`、`0`、`255`、空字符串、`0` |
| `MyHidConfig` | `vid`、`pid` | `0x034C`、`0x0368` |

默认值只在字段缺失时使用；当前代码不对映射范围、索引连续性或 VID/PID 一致性作额外校验。

## 旧格式兼容

旧 profile 使用固定按钮字段和 X/Y 轴字段。反序列化时，按钮和轴分别处理：对应动态字段是 JSON 数组时读取该数组（即使数组为空）；若字段缺失或不是数组，才尝试从该类的 legacy 字段重建。

### 按钮迁移

- 只有存在 `buttonUsagePage` 时才重建 legacy 按钮。
- `button_01Usage` 至 `button_07Usage` 分别生成映射；缺失时默认使用 `1` 至 `7`，值为 `0` 的项跳过。
- `buttonLinkCollection` 缺失时默认为 `0`。
- 名称生成为 `"Btn " + usage`，索引沿用字段序号 `0..6`；`usagePage` 使用 `buttonUsagePage`，缺失时默认 `0x09`。

### 轴迁移

- 只有存在 `axisUsagePage` 时才重建 legacy 轴。
- 总是生成 X 轴；`xUsage`、`axisLinkCollection`、`xLogicalMin`、`xLogicalMax` 缺失时分别默认为 `0x30`、`0`、`0`、`255`。
- 若 `enableYAxis` 为 `true`，从 X 映射复制生成 Y 轴，只将 usage 改为 `0x31`、名称改为 `"Y"`、索引改为 `1`。因此 Y 轴沿用 X 轴的 Usage Page、Link Collection 和逻辑范围。
- `xIdleTimeoutMs` 不参与当前反序列化。

`configToJson` 只写 VID、PID 和动态 `buttons[]`/`axes[]`，不会写 legacy 字段。通过程序加载旧配置后再执行保存，会写出当前动态格式。

## `DeviceProfileManager` 接口行为

| 方法 | 行为 |
|------|------|
| `loadProfile(vid, pid)` | 按传入 VID/PID 拼出文件名并读取。文件不存在返回 `nullopt`；读入或解析抛出异常时记录 `stderr` 并返回 `nullopt`。不校验 JSON 内嵌 VID/PID 是否与文件名匹配。 |
| `saveProfile(config, name)` | 按配置的 VID/PID 创建或覆盖 JSON 文件；捕获到异常时记录 `stderr` 并返回 `false`。当前未检查输出流写入后的状态，因此某些文件打开/写入失败可能仍返回 `true`。 |
| `listProfiles()` | 扫描目录中的普通 `.json` 文件；仅加入 JSON 中 VID 和 PID 均非零的项目。单文件读取或解析异常会被静默忽略。 |
| `deleteProfile(vid, pid)` | 删除对应文件；文件不存在或 `remove` 失败时返回 `false`，成功删除返回 `true`。 |
| `getProfilesDir()` | 返回 manager 保存的目录路径。 |

`listProfiles()` 使用 JSON 中的 VID/PID 作为 profile 身份，不要求文件名符合命名规则。`loadProfile()` 则通过传入 VID/PID 决定文件路径，但 `DeviceProfile.config` 的 VID/PID 从 JSON 内容读取（缺失时使用上述默认值）。

## 错误处理与并发

- 目录创建时传入的 `error_code` 当前未检查。
- `loadProfile` 对读取/解析异常有日志；`listProfiles` 单文件异常被静默跳过。
- `saveProfile` 捕获并记录异常，但未检查 `ofstream` 的最终状态；不能据其 `true` 返回值保证文件已完整写入。
- `deleteProfile` 的文件删除失败不写日志，只返回 `false`。
- `DeviceProfileManager` 没有内部锁；目前调用点在 CLI 主线程。若增加跨线程访问，需要另行同步。

## 建议验证场景

1. 当前格式写入后读取，比较配置字段和名称。
2. 动态按钮数组为空而存在 legacy 按钮字段时，确认动态数组优先，不迁移 legacy；动态 axes 同理。
3. 仅缺少按钮或轴其中一类动态数组时，确认另一类仍可独立从 legacy 字段迁移。
4. 测试缺失的 VID/PID、映射字段默认值，以及 `enableYAxis` 的迁移结果。
5. 检查缺失文件、损坏 JSON、零 VID/PID、目录不可写和删除失败时的实际返回值与日志行为。
