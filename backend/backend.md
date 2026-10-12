# Backend 模块

文件位置：`backend/`

## 模块组成

| 文件 | 职责 |
|------|------|
| `hid_input_backend.h/.cpp` | 公共 API：start/stop/tryGetLatest/setTargetVidPid/autoConfigure/getConfig |
| `device_manager.h/.cpp` | 目标设备跟踪、多集合扫描/增删、capability 索引、所有权判定 |
| `state_publisher.h/.cpp` | 线程安全状态发布（mutex 单生产者/单消费者） |
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
HidInputBackend::start()
    │
    ├─▶ RawInputWindow 创建隐藏窗口
    ├─▶ RawInputRegistry::registerAll() 通配注册
    ├─▶ 消息循环线程启动
    │
    ▼
WM_INPUT 回调 (在 RawInputWindow 线程)
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

### 线程安全
- `StatePublisher`：mutex 保护 `latest_`，单生产者（输入线程）/单消费者（主线程）
- `DeviceManager`：仅在输入线程访问，无锁
- `HidInputBackend` 公共方法：主线程调用，内部同步或委托线程安全组件

### 惰性绑定
- `WM_INPUT` 到达时，若 handle 不在 `devices_` 中，自动 `tryAddTargetDevice`
- 处理设备变更通知到达前已有输入报告的边缘情况

---

## HidInputBackend 公共接口

| 方法 | 线程 | 说明 |
|------|------|------|
| `start()` | 主线程 | 创建窗口、注册 Raw Input、启动消息循环线程 |
| `stop()` | 主线程 | 发送 WM_QUIT、等待线程结束、清理资源 |
| `tryGetLatest(out)` | 主线程 | 非阻塞读取最新快照，无数据返回 false |
| `setTargetVidPid(vid,pid)` | 主线程 | 切换目标设备，触发重新扫描 |
| `autoConfigure(vid,pid,outCaps)` | 主线程 | 完整自动检测流程，输出合并后的 capabilities |
| `getConfig()` | 主线程 | 返回当前适配器配置（含动态映射） |

---

## 测试要点

- 启动/停止不泄漏线程/窗口/句柄
- 选择设备后 `connected` 变 true，断开后变 false
- 复合设备：鼠标移动/点击/滚轮均能反映
- 切换设备后旧状态清零、新配置生效
- 并发 `tryGetLatest` 与 `publishState` 无数据竞争