# DeviceInfo

文件位置：`core/types/hid_types.h`（同文件内）

## 定义

```cpp
struct DeviceInfo {
    HANDLE handle = INVALID_HANDLE_VALUE;
    uint16_t vid = 0;
    uint16_t pid = 0;
    std::wstring name;
    USAGE usagePage = 0;
    USAGE usage = 0;
};
```

## 字段说明

| 字段 | 来源 | 用途 |
|------|------|------|
| `handle` | `RAWINPUTDEVICELIST.hDevice` | Raw Input 设备句柄，后续 `GetRawInputDeviceInfo` 使用 |
| `vid`/`pid` | `RID_DEVICE_INFO.hid.dwVendorId/ProductId` | 设备身份，供用户选择、Profile 匹配 |
| `name` | `GetRawInputDeviceInfo(..., RIDI_DEVICENAME)` | 设备路径（如 `\\?\HID#VID_034C&PID_0368#...`），调试显示 |
| `usagePage`/`usage` | `RID_DEVICE_INFO.hid.usUsagePage/Usage` | Top-level collection 用途（如 0x01/0x04=Mouse, 0x01/0x05=Joystick） |

## 使用流程

1. `DeviceEnumerator::enumerateAll()` 遍历 `GetRawInputDeviceList`，过滤 `RIM_TYPEHID`
2. 对每个 handle 调 `GetRawInputDeviceInfo(RIDI_DEVICEINFO)` 取 VID/PID/Usage
3. 调 `GetRawInputDeviceInfo(RIDI_DEVICENAME)` 取路径名
4. 返回 `vector<DeviceInfo>` 供 CLI `list` 命令显示、用户 `select` 索引

## 注意

- 同一物理设备可能对应多个 `DeviceInfo`（多 top-level collection，如鼠标+键盘复合设备）
- `handle` 仅在当前进程/会话有效，**不可持久化**
- 选择设备时以 `vid`+`pid` 为键，后端会聚合所有同 VID/PID 的 collection