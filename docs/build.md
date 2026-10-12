# 构建与开发指南

## 环境要求

- **Windows 10/11** (Raw Input + HID API)
- **Visual Studio 2022** (MSVC v143+) 或 **Build Tools for Visual Studio 2022**
- **CMake 3.20+**
- **C++17** 兼容编译器

---

## 快速开始

```cmd
cd D:\Fork\2dx-input-overlay
run_build.cmd
```

产物：`build\Release\single_hid_monitor.exe`

---

## 构建脚本详解

### `run_build.cmd`
```cmd
cmake -S . -B build
cmake --build build --config Release
```
- 单一构建目录 `build/`
- Release 配置优化
- 无需手动指定 generator，默认用 VS solution

### `CMakeLists.txt` 结构
```cmake
# 模块化源文件组织
set(CORE_TYPES_SRCS core/types/hid_types.h)
set(CORE_CAPABILITIES_SRCS core/capabilities/hid_capabilities.h core/capabilities/hid_capabilities.cpp)
set(CORE_PROFILES_SRCS core/profiles/*.h core/profiles/*.cpp)
set(PLATFORM_ENUMERATOR_SRCS platform/enumerator/*.h platform/enumerator/*.cpp)
set(PLATFORM_RAW_INPUT_SRCS platform/raw_input/*.h platform/raw_input/*.cpp)
set(PLATFORM_HID_SRCS platform/hid/*.h platform/hid/*.cpp)
set(BACKEND_SRCS backend/*.h backend/*.cpp)
set(ADAPTER_SRCS adapter/*.h adapter/*.cpp)
set(CLI_COMMANDS_SRCS cli/commands/*.h cli/commands/*.cpp)
set(CLI_FORMATTERS_SRCS cli/formatters/*.h cli/formatters/*.cpp)
set(CLI_MAIN_SRCS main.cpp)

add_executable(single_hid_monitor
    ${CORE_TYPES_SRCS} ${CORE_CAPABILITIES_SRCS} ${CORE_PROFILES_SRCS}
    ${PLATFORM_ENUMERATOR_SRCS} ${PLATFORM_RAW_INPUT_SRCS} ${PLATFORM_HID_SRCS}
    ${BACKEND_SRCS} ${ADAPTER_SRCS} ${CLI_COMMANDS_SRCS} ${CLI_FORMATTERS_SRCS}
    ${CLI_MAIN_SRCS}
)

target_include_directories(single_hid_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRD_PARTY_INCLUDE}  # nlohmann/json
)

target_link_libraries(single_hid_monitor PRIVATE hid)  # Windows hid.lib
```

**IDE 分组** (`source_group`)：Visual Studio 解决方案资源管理器按模块树显示

---

## 依赖管理

### nlohmann/json (vendored)
- 位置：`third_party/nlohmann/`
- 单头文件 `json.hpp` + 依赖
- 无外部包管理器，随源码分发
- CMake: `THIRD_PARTY_INCLUDE` 指向 `third_party/`

### Windows SDK
- `hidsdi.h` + `hid.lib` (HID 解析)
- `windows.h` + `user32.lib` (Raw Input)
- 默认链接，无需额外配置

---

## 常用构建命令

### 清理重建
```cmd
rmdir /s /q build
run_build.cmd
```

### Debug 构建
```cmd
cmake -S . -B build
cmake --build build --config Debug
```
产物：`build\Debug\single_hid_monitor.exe`

### 仅配置不编译
```cmd
cmake -S . -B build
```

### 指定 Generator (如需 Ninja)
```cmd
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

### 并行编译
```cmd
cmake --build build --config Release -- /m
```

---

## 运行与测试

### 交互式运行
```cmd
build\Release\single_hid_monitor.exe
```
命令：
- `l` - 列出所有 HID 设备
- `s N` - 选择设备 (索引)
- `p` - 打印当前动态映射配置
- `w [name]` - 保存 profile
- `q` - 退出

### Profile 目录
- 默认：工作目录下 `profiles/`
- 文件名：`<VID>_<PID>.json` (如 `034C_0368.json`)
- 可通过 `DeviceProfileManager` 构造函数指定自定义路径

---

## 开发工作流

### 新增模块步骤
1. 在对应层创建目录：`mkdir 新模块名`
2. 编写 `.h/.cpp` + `AGENTS.md` + 业务 `.md`
3. 在 `CMakeLists.txt` 增加 `set(NEW_MODULE_SRCS ...)` 并加入 `add_executable`
4. 如需 IDE 分组，增加 `source_group("path" FILES ${NEW_MODULE_SRCS})`
5. 运行 `run_build.cmd` 验证

### 修改核心类型 (`core/types/hid_types.h`)
**必须同步更新**：
- `core/types/hid_types.md` (字段表、计算规则)
- `core/profiles/profile_serialization.cpp` (JSON 映射)
- `adapter/config_mapper.cpp` (Caps→Config 转换)
- `backend/state_publisher.cpp` (State→Overlay 发布)
- `cli/formatters/*.cpp` (显示格式化)

### 代码规范
- **无注释**（除非必要），代码自述
- **单文件 ≤ 300 行**，圈复杂度 ≤ 10
- **错误处理**：返回 `bool`/`optional`，不抛异常，`stderr` 记录
- **线程安全**：明确标注哪些线程访问，用 `mutex`/`atomic` 保护

---

## 故障排查

### 编译错误：找不到 nlohmann/json
```cmd
# 检查 third_party/nlohmann/json.hpp 是否存在
dir third_party\nlohmann\json.hpp
```

### 编译错误：hid.lib 链接失败
- 确保 Windows SDK 安装 (Visual Studio Installer → 单个组件 → Windows 10 SDK)
- 项目属性 → 链接器 → 输入 → 附加依赖项 包含 `hid.lib`

### 运行时：无设备列出
- 确认有 HID 设备连接 (设备管理器 → 人体学输入设备)
- 以管理员运行 (某些设备需要)

### 运行时：选择设备后 connected=0
- 设备可能无 Input report，或 VID/PID 匹配但无 preparsed data
- 尝试 `s N` 选择其他同设备的 collection (同 VID/PID 不同 Usage)

### VS IntelliSense 报红但编译通过
- 重新生成解决方案：`cmake -S . -B build` 刷新
- 或删除 `.vs/` 目录重启 VS

---

## CI/CD 集成建议

```yaml
# GitHub Actions 示例
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S . -B build
      - name: Build
        run: cmake --build build --config Release
      - name: Test (smoke)
        run: |
          # 无设备环境下仅验证启动不崩溃
          timeout 5 build\Release\single_hid_monitor.exe <<< "q" || true
```

---

## 目录结构速查

```
2dx-input-overlay/
├── AGENTS.md              # 顶层索引
├── README.md
├── CMakeLists.txt
├── run_build.cmd
├── third_party/nlohmann/
├── core/
│   ├── types/
│   ├── profiles/
│   └── capabilities/
├── platform/
│   ├── enumerator/
│   ├── raw_input/
│   └── hid/
├── backend/
├── adapter/
├── cli/
│   ├── commands/
│   └── formatters/
├── docs/
│   ├── architecture.md
│   ├── data_flow.md
│   └── build.md
└── main.cpp
```

---

## 版本与发布

- 当前无版本号机制，按需打 tag
- 单一可执行文件部署：`single_hid_monitor.exe` + `profiles/` 目录
- 无运行时依赖 (静态链接 CRT: `/MT` 需在 CMake 中设置 `CMAKE_MSVC_RUNTIME_LIBRARY`)