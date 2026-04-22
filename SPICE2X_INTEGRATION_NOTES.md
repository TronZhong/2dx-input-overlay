# spice2x 最小接入说明（内部，冻结）

> 状态：归档预研文档。
> 
> 当前项目主目标为 OBS 实时输入显示插件，本文件不作为当前迭代执行清单，仅保留历史接入思路与落点参考。

本文件仅记录当前仓库向 spice2x 的最小接入落点，不面向用户。

## 目标输入语义

- 设备过滤：VID/PID。
- 状态字段：7 按钮 + `xDirection`（-1/0/1）。
- 不在本阶段扩展通用映射层与 UI 配置。

## 最小接入点

1. HID 扫描阶段
- 文件：`src/spice2x/rawinput/rawinput.cpp`
- 位置：`devices_scan_rawinput(...)` 的 HID 分支
- 动作：检测目标 VID/PID，记录目标设备句柄与日志

2. HID 输入处理阶段
- 文件：`src/spice2x/rawinput/rawinput.cpp`
- 位置：`input_wnd_proc(...)` 的 `WM_INPUT` HID 分支
- 动作：传入 preparsed data + raw report，调用 `MyHidAdapter::updateFromReport(...)`

3. 游戏输入映射阶段
- 文件：`src/spice2x/cfg/button.h`、`src/spice2x/cfg/api.cpp`
- 动作：将 7 键状态与方向状态映射到游戏定义输入

## 推荐落盘结构

- 接入层仅落盘一个快照结构，字段与 `HidOverlayState` 保持同语义。
- 采集线程负责写快照，消费侧只读快照，不直接触达 HID API。

## 最小伪代码（WM_INPUT HID 分支）

```cpp
if (isMyTargetDevice(device)) {
    MyHidState state;
    const bool ok = adapter.updateFromReport(
        reinterpret_cast<PHIDP_PREPARSED_DATA>(device.hidInfo->preparsed_data.get()),
        reinterpret_cast<const uint8_t *>(data_hid.bRawData),
        data_hid.dwSizeHid,
        state);

    if (ok) {
        // 统一写入快照（7键 + xDirection）
        // 后续 button-read 路径或渲染路径只消费快照。
    }
}
```

## 回退策略

- 若目标设备未匹配：保持默认输入路径，不影响现有设备。
- 若报告解析失败：丢弃该帧并保留上一帧快照，不中断主循环。
- 若设备断开：发布 disconnected 状态，等待热插拔恢复。
