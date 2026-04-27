# 设备信息模板（内部执行）

用于回填当前仓库所需的最小参数，不面向用户。

## 1) 设备身份

- Device path：运行时由 Raw Input 枚举，当前不固化为静态路径
- VID (hex, 4位)：0x034C
- PID (hex, 4位)：0x0368
- Product name：按系统枚举结果（本轮不作为接入判定条件）
- Manufacturer：按系统枚举结果（本轮不作为接入判定条件）

## 2) 顶层能力

- Top Usage Page (hex)：0x01（Generic Desktop）
- Top Usage (hex)：N/A（当前路径按按钮页与轴页参数判定）

## 3) 按钮参数（7键）

- Button Usage Page (hex，通常 0x09)：0x09
- Button Link Collection (decimal，通常 0)：0
- Button01 Usage (hex)：0x01
- Button02 Usage (hex)：0x02
- Button03 Usage (hex)：0x03
- Button04 Usage (hex)：0x04
- Button05 Usage (hex)：0x05
- Button06 Usage (hex)：0x06
- Button07 Usage (hex)：0x07

## 4) 轴参数（当前仅 X）

- Axis Usage Page (hex，通常 0x01)：0x01
- Axis Link Collection (decimal，通常 0)：0
- X Usage (hex，常见 0x30)：0x30
- X Logical Min：0
- X Logical Max：255

## 5) 运行时验证

- `HidP_GetUsages` 能稳定返回 7 键状态：yes
- `HidP_GetUsageValue` 能稳定返回 X 值：yes
- 连续转动时 `xDirection` 输出稳定（-1/0/1）：yes
- 设备断开后状态可回落到 disconnected：yes

## 6) 回填目标（对应代码）

- `my_hid_adapter.h` 中 `MyHidConfig` 默认值已按实测更新：yes
- `obs-plugintemplate/src/input-overlay-source.c` 默认参数已同步：yes

## 7) 结论

- 可直接走现有 HID 路径接入：yes
- 需要 vendor-specific 分支：no
- 备注：本轮按默认参数（VID/PID + 7键 + X轴）完成主程序与 OBS 路径验证，行为符合预期。
