# Raw Input 平台层

文件位置：`platform/raw_input/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `raw_input_window.h/.cpp` | 隐藏窗口创建、消息循环、WM_INPUT/WM_INPUT_DEVICE_CHANGE/WM_DESTRIBUTE 分发 |
| `raw_input_registry.h/.cpp` | `RegisterRawInputDevices` 封装（通配注册所有 HID） |
| `raw_input_parser.h/.cpp` | `GetRawInputData` 解析、预解析数据获取、设备名/信息查询 |

---

## RawInputWindow

### 职责
- 注册窗口类 `HidInputBackendWnd`
- 创建隐藏窗口（`CreateWindowExW`，`lpCreateParams=this`）
- 消息循环：`GetMessage` / `TranslateMessage` / `DispatchMessage`
- 静态窗口过程 `staticWndProc` → 取 `GWLP_USERDATA` 获取实例 → 调用 `handler_` 回调

### 接口
```cpp
using MessageHandler = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;
explicit RawInputWindow(MessageHandler handler);
bool create(HINSTANCE instance);
void destroy();
HWND hwnd() const;
DWORD threadId() const;
void runMessageLoop(std::atomic<bool>& running);
```

### 关键点
- `WM_CREATE` 时从 `CREATESTRUCT.lpCreateParams` 取回 `this` 存入 `GWLP_USERDATA`
- `ready_event_` 用于主线程等待窗口创建完成
- `destroy()` 中 `DestroyWindow` + `UnregisterClass` + `CloseHandle(ready_event_)`
- 消息循环在独立线程运行，`running` 原子标志控制退出

---

## RawInputRegistry

### 职责
封装 `RegisterRawInputDevices`，当前仅需**通配注册**（接收所有 HID 设备输入）。

### 接口
```cpp
static bool registerAll(HWND targetHwnd);  // usUsagePage=0, usUsage=0, RIDEV_INPUTSINK|RIDEV_DEVNOTIFY
static bool registerSpecific(HWND, USHORT, USHORT); // 保留兼容，新架构不使用
```

### 关键点
- `RIDEV_INPUTSINK`：非前台窗口也能收到输入
- `RIDEV_DEVNOTIFY`：启用 `WM_INPUT_DEVICE_CHANGE` 热插拔通知
- 通配注册后，设备是否处理由上层 `device_manager` 的 VID/PID 过滤决定

---

## RawInputParser

### 职责
将 `WM_INPUT` 的 `LPARAM` 解析为结构化数据，提供预解析数据获取。

### 接口
```cpp
struct RawInputData {
    HANDLE deviceHandle;
    vector<uint8_t> rawData;  // raw->data.hid.bRawData
    UINT dataSize;
};

static bool parseRawInput(LPARAM lParam, RawInputData& out);
static bool getPreparsedData(HANDLE handle, vector<uint8_t>& out);
static string getDeviceName(HANDLE handle);
static bool getDeviceInfo(HANDLE handle, RID_DEVICE_INFO& out);
```

### 关键点
- `parseRawInput`：两次调用 `GetRawInputData`（先取 size，再取数据），校验 `RIM_TYPEHID`
- `getPreparsedData`：两次调用 `GetRawInputDeviceInfo(RIDI_PREPARSEDDATA)`，返回字节向量供 `HidP_*` 使用
- 宽字符转 UTF-8 统一用 `wideToUtf8` 内部函数

---

## 使用流程

1. `backend/device_manager.cpp` 构造 `RawInputWindow`，传入处理回调
2. `RawInputWindow::create` → `RawInputRegistry::registerAll(hwnd)`
3. 工作线程 `runMessageLoop(running)` 循环分发消息
4. 回调中：
   - `WM_INPUT` → `RawInputParser::parseRawInput` → 得到 `deviceHandle` + `rawData` → 上层解析
   - `WM_INPUT_DEVICE_CHANGE` → `GIDC_ARRIVAL/REMOVAL` → 上层增删设备
5. 销毁时 `RawInputWindow::destroy()`

---

## 测试要点

- 窗口创建/销毁不泄漏句柄/类注册
- 通配注册后，鼠标/键盘/手柄均产生 `WM_INPUT`
- `WM_INPUT_DEVICE_CHANGE` 热插拔正确触发
- `parseRawInput` 对非 HID 输入返回 false
- 预解析数据获取成功，长度 > 0