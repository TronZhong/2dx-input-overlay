# Raw Input 平台层

文件位置：`platform/raw_input/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `raw_input_window.h/.cpp` | 隐藏窗口创建、消息循环及窗口消息回调 |
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
- `ready_event_` 在 `runMessageLoop()` 开始时置位；它不单独确认窗口创建或 Raw Input 注册成功
- `destroy()` 中 `DestroyWindow` + `UnregisterClass` + `CloseHandle(ready_event_)`
- 后端当前先在调用 `start()` 的线程创建窗口并注册 Raw Input，再在 worker 线程调用 `runMessageLoop()`。Win32 窗口由创建线程拥有，消息队列也属于该线程；worker 线程上的 `GetMessage` 不等同于泵送该窗口所属线程的队列。当前线程安排存在问题，不能据此承诺 Raw Input 回调在 worker 正常处理。

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

1. `HidInputBackend::start()` 构造 `RawInputWindow` 并传入处理回调
2. 调用线程执行 `RawInputWindow::create` → `RawInputRegistry::registerAll(hwnd)`
3. 随后启动的工作线程执行 `runMessageLoop(running)`；注意窗口与消息循环的线程归属问题见上
4. 回调中：
   - `WM_INPUT` → `RawInputParser::parseRawInput` → 得到 `deviceHandle` + `rawData` → 上层解析
   - `WM_INPUT_DEVICE_CHANGE` → `GIDC_ARRIVAL/REMOVAL` → 上层增删设备
5. 销毁时 `RawInputWindow::destroy()`

---

## 测试要点

- 窗口创建/销毁不泄漏句柄/类注册
- 通配注册后，鼠标/键盘/手柄均产生 `WM_INPUT`
- `WM_INPUT_DEVICE_CHANGE` 热插拔行为需在窗口消息循环线程修正后验证
- `parseRawInput` 对非 HID 输入返回 false
- 预解析数据获取成功，长度 > 0