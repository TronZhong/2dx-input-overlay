# 设备信息模板（内部执行）

用于回填当前仓库所需的最小参数，不面向用户。

## 1) 设备身份

- Device path：
- VID (hex, 4位)：
- PID (hex, 4位)：
- Product name：
- Manufacturer：

## 2) 顶层能力

- Top Usage Page (hex)：
- Top Usage (hex)：

## 3) 按钮参数（7键）

- Button Usage Page (hex，通常 0x09)：
- Button Link Collection (decimal，通常 0)：
- Button01 Usage (hex)：
- Button02 Usage (hex)：
- Button03 Usage (hex)：
- Button04 Usage (hex)：
- Button05 Usage (hex)：
- Button06 Usage (hex)：
- Button07 Usage (hex)：

## 4) 轴参数（当前仅 X）

- Axis Usage Page (hex，通常 0x01)：
- Axis Link Collection (decimal，通常 0)：
- X Usage (hex，常见 0x30)：
- X Logical Min：
- X Logical Max：

## 5) 运行时验证

- `HidP_GetUsages` 能稳定返回 7 键状态：yes / no
- `HidP_GetUsageValue` 能稳定返回 X 值：yes / no
- 连续转动时 `xDirection` 输出稳定（-1/0/1）：yes / no
- 设备断开后状态可回落到 disconnected：yes / no

## 6) 回填目标（对应代码）

- `my_hid_adapter.h` 中 `MyHidConfig` 默认值已按实测更新：yes / no
- `obs-plugintemplate/src/input-overlay-source.c` 默认参数已同步：yes / no

## 7) 结论

- 可直接走现有 HID 路径接入：yes / no
- 需要 vendor-specific 分支：yes / no
- 备注：
