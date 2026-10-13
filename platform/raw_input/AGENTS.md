# platform/raw_input 模块文档索引

## 文件清单

| 文件 | 说明 |
|------|------|
| `raw_input_window.h/.cpp` | 隐藏窗口、消息循环、消息分发回调 |
| `raw_input_registry.h/.cpp` | RegisterRawInputDevices 封装（通配注册） |
| `raw_input_parser.h/.cpp` | WM_INPUT 解析、预解析数据、设备名/信息获取 |
| `raw_input.md` | 详细设计、接口、关键点、测试要点 |

## 依赖关系

- 依赖：Windows SDK `<windows.h>` `<hidsdi.h>`、`core/types/hid_types.h`（仅 `HANDLE` 等类型）
- 被依赖：`backend/device_manager.cpp`、`backend/hid_input_backend.cpp`

## 设计约束

1. **仅封装 Win32 API**：无业务逻辑、无设备过滤
2. **回调模式**：`RawInputWindow` 接收 `MessageHandler`，上层实现具体处理
3. **当前线程安排限制**：`HidInputBackend::start()` 在调用线程创建窗口，之后才启动 worker 并运行消息循环。Win32 窗口消息归属创建线程；窗口和消息泵线程不一致，不能假定上层回调在 worker 中正常执行。
4. **错误处理**：按各方法实际返回值处理可检查的 API 错误；当前并非所有 Win32 调用都检查返回值

## 修改指南

- 修改消息处理逻辑 → 仅改 `backend/device_manager.cpp` 的回调实现，`raw_input_window` 不变
- 新增 Raw Input 标志位 → 修改 `raw_input_registry.cpp` `registerAll`
- 变更预解析数据获取方式 → 修改 `raw_input_parser.cpp` `getPreparsedData`
- 增加设备信息查询 → 扩展 `RawInputParser` 静态方法