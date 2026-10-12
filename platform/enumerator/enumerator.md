# DeviceEnumerator

文件位置：`platform/enumerator/device_enumerator.h/.cpp`

## 职责

枚举系统中所有 HID 设备，返回 `DeviceInfo` 列表供 CLI `list` 命令显示、用户选择。

---

## 算法

```cpp
vector<DeviceInfo> enumerateAll() {
    // 1. GetRawInputDeviceList(nullptr, &count) -> 获取设备数量
    // 2. GetRawInputDeviceList(list.data(), &count) -> 填充 RAWINPUTDEVICELIST 数组
    // 3. 遍历 list:
    //    - 过滤 dwType != RIM_TYPEHID
    //    - GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO) -> RID_DEVICE_INFO
    //      * 过滤 info.dwType != RIM_TYPEHID
    //      * 提取 vid/pid/usagePage/usage
    //    - GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME) -> 设备路径名
    //    - 构造 DeviceInfo 入结果向量
}
```

---

## 关键点

- `RAWINPUTDEVICELIST.hDevice` 即 Raw Input 句柄，后续 `GetRawInputDeviceInfo` 使用
- `RID_DEVICE_INFO.hid.dwVendorId/ProductId` 为 32 位值，截取低 16 位为 `uint16_t`
- `RIDI_DEVICENAME` 返回宽字符路径（如 `\\?\HID#VID_034C&PID_0368#...`），转 UTF-8 存入 `DeviceInfo.name`（`wstring` 保留原始）
- 同一物理设备可能有多个 top-level collection → 返回多个 `DeviceInfo`（相同 VID/PID，不同 `usagePage`/`usage`/`handle`）
- 错误处理：单个设备查询失败时 `continue`，不中断整体枚举

---

## 使用流程

1. `cli/commands/list_cmd.cpp` 调用 `DeviceEnumerator::enumerateAll()`
2. `cli/formatters/device_formatter.cpp` 格式化输出
3. 用户执行 `s N` 选择索引 → `cli/commands/select_cmd.cpp` 取 `devices[N]` 的 `vid`/`pid` 传给后端

---

## 测试要点

- 无 HID 设备时返回空向量
- 多设备时按枚举顺序返回（索引稳定）
- 复合设备（鼠标+键盘）返回多条，VID/PID 相同，Usage 不同
- 设备拔插后重新枚举能反映最新状态