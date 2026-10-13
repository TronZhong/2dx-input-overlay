# Backend 模块

文件位置：`backend/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `hid_input_backend.h/.cpp` | 公共 API：start/stop/tryGetLatest/setTargetVidPid/autoConfigure/getConfig |
| `device_manager.h/.cpp` | 目标设备跟踪、多集合扫描/增删、capability 索引、所有权判定 |
| `state_publisher.h/.cpp` | 用 mutex 保护最新状态快照 |
| `auto_config.h/.cpp` | 自动检测编排：扫描设备、聚合 capabilities、调用 adapter.autoDetect |

---

## 数据流

```
用户选择设备 (s N)
    │
    ▼
HidInputBackend::autoConfigure(vid, pid)
    │
    ├─▶ DeviceManager::scanTargetDevices()  ──▶ 遍历所有匹配 VID/PID 的 Raw Input handle
    │       │
    │       ├─▶ tryAddTargetDevice() ──▶ loadPreparsedData() + indexDeviceCapabilities()
    │       │
    │       └─▶ devices_ map 填充 TargetHidDevice {handle, name, preparsed, buttonUsages, axisUsages}
    │
    ├─▶ AutoConfig::configure()
    │       │
    │       ├─▶ 遍历 devices_，对每个 preparsed 调 CapabilityInspector::enumerate()
    │       ├─▶ 去重合并 buttons (usage+linkCollection) / values (usagePage+usage+linkCollection)
    │       ├─▶ adapter.autoDetect(merged) ──▶ 生成 MyHidConfig.buttons/axes
    │       └─▶ 可选输出 merged caps 供 CLI 打印表头
    │
    ▼
HidInputBackend::start()（CLI 启动时已调用）
    │
    ├─▶ 调用线程创建 RawInputWindow 并通配注册
    ├─▶ 随后启动消息循环线程
    │
    ▼
WM_INPUT 回调（设计中的处理路径；当前窗口与消息循环线程不一致）
    │
    ├─▶ RawInputParser::parseRawInput() ──▶ deviceHandle + rawData
    ├─▶ DeviceManager::tryAddTargetDevice() (懒加载)
    ├─▶ adapter.updateFromReport(preparsed, rawData, slice)
    ├─▶ DeviceManager::ownsButton/ownsAxis 判定所有权
    ├─▶ 合并 slice 到 master state (仅所有集合拥有的信号)
    ├─▶ StatePublisher::publishState(master, config)
    │
    ▼
CLI 调用 tryGetLatest() 读取最新快照
```

---

## 关键设计

### 多集合合并
- 复合设备（鼠标+键盘、手柄+音频）在 Raw Input 中表现为多个 handle
- `autoConfigure` 聚合所有集合的 capabilities（去重）
- 实时解析时，每个集合用自己的 preparsed 解析，再按“所有权”合并到 master state
- `TargetHidDevice.buttonUsages/axisUsages` 记录各集合拥有的信号，防止跨集合清零

### 并发与当前限制
- `StatePublisher` 的 mutex 只保护 `latest_` 和 `has_latest_`；不保护传入对象或其他 backend 成员。
- `configMutex` 仅由 `getConfig`、`setTargetVidPid` 和 `autoConfigure` 获取。Raw Input 回调读取或修改配置、adapter、DeviceManager、masterState 时不获取该锁。
- 设备选择命令在 worker 已启动后仍可能在 CLI 线程调用 `autoConfigure`/`setTargetVidPid`；它们访问的设备状态也可能正由消息回调使用，因此当前实现不具备可靠的并发设备切换保护。
- 窗口在调用 `start()` 的线程创建，但消息循环在后启 worker 上运行。Win32 窗口消息归属创建窗口的线程，因此此安排不能保证窗口消息由 worker 正确泵送。

### 惰性绑定
- `WM_INPUT` 到达时，若 handle 不在 `devices_` 中，自动 `tryAddTargetDevice`
- 处理设备变更通知到达前已有输入报告的边缘情况

---

## HidInputBackend 公共接口

| 方法 | 线程 | 说明 |
|------|------|------|
| `start()` | CLI 调用线程 | 创建窗口、注册 Raw Input、启动消息循环线程；当前窗口与消息泵线程不同，且 public 方法即使内部启动失败仍返回 `true` |
| `stop()` | 主线程 | 发送 WM_QUIT、等待线程结束、清理资源 |
| `tryGetLatest(out)` | 主线程 | 非阻塞读取最新快照，无数据返回 false |
| `setTargetVidPid(vid,pid)` | 主线程 | 设置 VID/PID、清理内部 master state 并重新扫描；不应用映射，也不立即向 publisher 发布清空快照 |
| `autoConfigure(vid,pid,outCaps)` | CLI 调用线程 | 扫描目标设备、聚合 capabilities 并尝试生成配置；不启动后端 |
| `getConfig()` | 主线程 | 返回当前适配器配置（含动态映射） |

---

## 测试要点

- 启动/停止不泄漏线程/窗口/句柄
- 能解析到 Raw Input 报告后 `connected` 置 true；目标 handle 移除且没有其他目标 handle 时发布 disconnected 快照。现有实现不会在仍有其他 handle 时清理被移除集合所对应的映射状态。
- 复合设备：鼠标移动/点击/滚轮均能反映
- 切换设备后应验证发布快照是否更新；当前只清理内部 master state，不会立即发布断开/清空快照
- `tryGetLatest` 与 `publishState` 的快照访问由 publisher mutex 同步；这不代表设备切换与报告处理的并发访问安全