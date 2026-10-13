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
- 未指定 generator，由 CMake 按当前环境选择默认 generator；只有环境默认选中 Visual Studio 时才会生成 VS solution。
- `run_build.cmd` 展示的产物路径 `build\Release\...` 适用于 Visual Studio 等多配置 generator；本仓库的标准 Windows 构建环境使用 Visual Studio。

### `CMakeLists.txt` 结构

下方为模块列表摘要。CMake 文件使用逐个列出的源文件，不使用通配符自动收集：

```cmake
set(CORE_TYPES_SRCS core/types/hid_types.h)
set(CORE_CAPABILITIES_SRCS
    core/capabilities/hid_capabilities.h
    core/capabilities/hid_capabilities.cpp
)
# 其余 CORE_PROFILES_SRCS、PLATFORM_*_SRCS、BACKEND_SRCS、ADAPTER_SRCS、
# CLI_COMMANDS_SRCS、CLI_FORMATTERS_SRCS 同样逐文件列在 CMakeLists.txt 中。
add_executable(single_hid_monitor
    ${CORE_TYPES_SRCS}
    ${CORE_CAPABILITIES_SRCS}
    # 其余模块变量与 main.cpp
)

target_include_directories(single_hid_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRD_PARTY_INCLUDE}
)
if (WIN32)
    target_link_libraries(single_hid_monitor PRIVATE hid)
endif()
```

以上代码展示组织方式，不是可独立复制的完整构建脚本；实际源文件清单以仓库根目录 `CMakeLists.txt` 为准。`source_group` 仅在 MSVC 下设置 IDE 分组。

---

## 依赖管理

### nlohmann/json (vendored)
- 位置：`third_party/nlohmann/`
- 单头文件 `json.hpp` + 依赖
- 无外部包管理器，随源码分发
- CMake: `THIRD_PARTY_INCLUDE` 指向 `third_party/`

### Windows SDK
- `hidsdi.h` + `hid.lib` (HID 解析)
- `windows.h` + Raw Input API（CMake 未显式添加 `user32`；Windows/MSVC 的默认链接环境提供相关系统库）

---

## 常用构建命令

### 清理重建
```cmd
cmake --build build --config Release --clean-first
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
cmake -S . -B build-ninja -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
```
Ninja 是单配置 generator，产物为 `build-ninja\single_hid_monitor.exe`；`--config Release` 不会为单配置 generator 切换构建类型。此示例不使用 `run_build.cmd` 预期的 `build\Release\...` 路径。

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
- 默认：进程当前工作目录下 `profiles/`
- 文件名：`<VID>_<PID>.json` (如 `034C_0368.json`)
- `DeviceProfileManager` 构造函数支持传入自定义路径；当前 CLI 使用默认路径

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

### 开发建议
- **无注释**（除非必要），代码自述
- **单文件 ≤ 300 行**，圈复杂度 ≤ 10
- **错误处理**：优先使用 `bool`/`optional` 表达可恢复失败并明确记录错误；JSON/文件系统等 API 仍可能抛异常，应按调用处现有策略处理
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
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S . -B build
      - name: Build
        run: cmake --build build --config Release
```

仓库当前未配置自动化测试目标；以上 CI 仅配置并构建程序，不代表设备输入行为已通过实机验证。

---

## 目录结构速查

```
2dx-input-overlay/
├── AGENTS.md              # 仓库级开发说明
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
│   ├── build.md
│   └── index.md
└── main.cpp
```

---

## 版本与发布

- 当前无版本号机制，按需打 tag
- 单一可执行文件部署：`single_hid_monitor.exe` + `profiles/` 目录
- 使用 Windows 系统 HID/Raw Input API；CMake 未设置静态 CRT（`/MT`），因此不可据此文档宣称静态链接 CRT 或完全无运行时依赖。