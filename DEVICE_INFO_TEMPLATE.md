# 设备信息模板（内部执行）

## 参数修改与检测入口索引（先看这里）

说明：当前程序已支持**自动检测**设备能力。选择设备（`s N`）后，程序用设备 HID 描述符自动枚举全部按钮与轴值 usage（含 logical min/max），生成动态映射，无需手填参数。下表为自动检测之外仍需人工确认/覆盖的入口。

1. 自动检测结果
- 入口：程序运行后 `s N` 选择设备，自动打印检测到的全部按钮/轴值与范围。
- 结果来源：`HidCapabilities`（`HidP_GetButtonCaps`/`HidP_GetValueCaps`）。

2. 动态映射存储
- 文件：`my_hid_adapter.h`（`MyHidConfig.buttons` / `.axes`）、`DeviceProfile`（`profiles/<vid>_<pid>.json`）。

3. legacy 固定默认值（自动检测失败或未检测时的回退基线）
- 文件：`my_hid_adapter.h`（`struct MyHidConfig` 的 7+1 固定字段默认值）。

4. 文档同步
- 文件：`DEVICE_INFO_TEMPLATE.md`，更新下面各节实测值。

## 1) 设备身份

- Device path：运行时由 Raw Input 枚举，不固化为静态路径。
- VID (hex, 4位)：0x034C
- PID (hex, 4位)：0x0368
- Product name / Manufacturer：按系统枚举打印，不作为接入判定条件。

## 2) 自动检测示例

以下为参考设备（GAMO2 PhoenixWan+/LMT HID 模式）检测到的能力示例，供手工校准或比对：

- 按钮数：7
- 按钮 usage 序列：0x01..0x07（Button page 0x09，Link Collection 0）
- 轴值/标量：X
- X usage：0x30（Generic Desktop page）
- X Logical Min / Max：0 / 255

实际以 `s N` 的检测输出为准。

## 3) 运行时验证

- `s N` 自动检测能列出目标设备全部按钮/轴值：yes
- 按压各按钮均能反映到状态输出：yes
- 转动轴值时归一化与方向输出稳定：yes
- 设备断开后状态回落到 disconnected：yes
- `w` 保存 profile、再次 `s N` 自动加载映射生效：yes

## 4) 结论

- 是否直接走自动检测生成映射：yes
- 需要手工覆盖的例外分支：no（如有则在此说明）
- 备注：当前主流程为“选设备 -> 自动检测 -> 动态映射解析 -> 保存/加载 profile”。
