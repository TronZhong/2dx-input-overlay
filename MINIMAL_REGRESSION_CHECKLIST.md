# 最小回归验证清单（文档级）

最后更新时间：2026-04-27

适用范围：当前迭代最小闭环验证。
目标：覆盖“主工程构建运行 + 设备输入验证 + OBS 最小加载验证”三段流程。

---

## A. 主工程构建与运行

### A1 构建

步骤：
1. `Set-Location d:/Fork/2dx-input-overlay`
2. `cmake -S . -B build`
3. `cmake --build build --config Release`

期望结果：
- 配置与编译返回码均为 0。
- 生成 `build/Release/single_hid_monitor.exe`。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

### A2 运行

步骤：
1. `./build/Release/single_hid_monitor.exe`
2. 保持程序运行 30 秒，无操作输入。

期望结果：
- 程序不崩溃。
- 日志可见设备连接状态或等待状态。
- 空闲时无异常刷屏。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

---

## B. 设备输入验证（7 键 + X 方向）

### B1 按键

步骤：
1. 依次按下 7 个按键，每次按住 1 秒后松开。
2. 观察输出状态变化。

期望结果：
- 7 个按键均能被单独识别。
- 无明显漏判（按了没反应）和误判（没按却触发）。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

### B2 方向

步骤：
1. 向左旋转 2 秒，再向右旋转 2 秒。
2. 停止输入并观察 1 秒。

期望结果：
- 左转时 `xDirection = -1`。
- 右转时 `xDirection = 1`。
- 停止后在 `xIdleTimeoutMs` 之后回落到 `0`。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

### B3 热插拔与断连

步骤：
1. 运行期间拔掉设备。
2. 观察状态回落。
3. 重新插入设备并再次触发输入。

期望结果：
- 断开后状态回落 `connected = false`。
- 重连后恢复实时更新。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

---

## C. OBS 最小加载验证

前置：`obs-plugintemplate/.deps` 依赖包完整且哈希通过。

### C1 插件构建

步骤：
1. `Set-Location d:/Fork/2dx-input-overlay/obs-plugintemplate`
2. 若存在中断残留，先执行 `Remove-Item .deps/.fetch-deps.lock -Force -ErrorAction SilentlyContinue`
3. 若存在中断残留，执行 `Get-ChildItem .deps -Filter *.part -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1`
5. `cmd /c run_build.cmd`

期望结果：
- 第 4 步输出 3 个依赖均为 `PASS`。
- 第 5 步构建成功，末尾出现 `[OK] OBS plugin build completed via preset windows-x64.`

失败补记：
- 若仅需验证失败路径，可临时将第 4 步改为 `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/fetch-deps.ps1 -MaxRetry 1`，但该命令不作为正式回归通过标准。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：

### C2 OBS 加载

步骤：
1. 将生成的插件二进制放入 OBS 对应插件目录（按本机 OBS 版本目录结构）。
2. 启动 OBS，新增 Source：`2DX Input Overlay`。
3. 在属性中设置 VID/PID 与 usage 参数。

期望结果：
- Source 可创建。
- 画面按钮与方向条随输入实时变化。
- 断连时显示回落，重连后恢复。

实测结果（填写）：
- 日期：
- 执行人：
- 结果：通过 / 失败
- 备注：
