# Device Info Template (Fill This First)

Use this as a checklist before writing integration code.

## Identity

- Device path:HID\VID_034C&PID_0368&MI_01\8&363C07EC&0&0000
- VID (hex):034
- PID (hex):0368
- Product name:HID-compliant game controller
- Manufacturer:(标准系统设备)

## Top-level HID capability

- Top Usage Page (hex):
- Top Usage (hex):

## Buttons you want

- Start button usage page (hex):
- Start button usage (hex):
- Service button usage page (hex):
- Service button usage (hex):
- Link collection for buttons (decimal, usually 0):

## Axes you want

- X axis usage page (hex):
- X axis usage (hex):
- X logical min:
- X logical max:
- Y axis usage page (hex):
- Y axis usage (hex):
- Y logical min:
- Y logical max:
- Link collection for axes (decimal, usually 0):

## Runtime checks

- HidP_GetUsages returns start/service usages: yes or no
- HidP_GetUsageValue returns X value: yes or no
- HidP_GetUsageValue returns Y value: yes or no

## Decision

- Can be integrated via existing HID path directly: yes or no
- Needs vendor-specific special case: yes or no
