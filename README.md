# AnyClaw

**OpenClaw Windows Desktop Manager** — 系统托盘常驻的 OpenClaw 管理工具

AnyClaw 是 OpenClaw 的 Windows 桌面管家，提供图形化界面管理 OpenClaw Gateway 的启动、停止、配置和健康监控。

## 功能特性

- 🖥️ **系统托盘常驻** — 轻量级运行，不占用任务栏
- 🟢 **健康状态监控** — 实时检测 Gateway 运行状态，图标颜色即时反馈
- ⚙️ **图形化设置** — ImGui 驱动的设置界面，配置端口、模型、主题等
- 🚀 **Gateway 管理** — 一键启动/停止/重启 OpenClaw Gateway
- 🔧 **自动安装** — 检测 OpenClaw 安装状态，支持 npm 全局安装
- 🌙 **开机自启** — 可配置 Windows 开机自动启动
- 🔔 **桌面通知** — 状态变更时气泡提示
- 🎨 **深色主题** — 适配 Windows 深色模式

## 截图

<!-- 截图占位 -->
![AnyClaw Settings](docs/screenshots/settings.png)

## 构建

### 依赖

- Visual Studio 2019 或 2022（含 C++17 支持）
- CMake 3.16+
- vcpkg（推荐，用于管理依赖）

### 依赖库

| 库 | 用途 | 获取方式 |
|---|---|---|
| GLFW 3.x | 窗口管理 | vcpkg / 源码编译 |
| Dear ImGui | GUI 框架 | 源码包含 |
| OpenGL 3.3+ | 渲染 | 系统自带 |

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/aetheros/anyclaw.git
cd anyclaw

# 使用 vcpkg 安装依赖
vcpkg install glfw3:x64-windows

# CMake 构建
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# 输出文件
# build\Release\AnyClaw.exe
```

### 手动构建（无 CMake）

```powershell
# 在 VS Developer Command Prompt 中
cl /std:c++17 /EHsc /I"thirdparty\imgui" /I"thirdparty\glfw\include" ^
   src\main.cpp src\tray.cpp src\settings.cpp ^
   thirdparty\imgui\imgui*.cpp ^
   /link glfw3.lib opengl32.lib shell32.lib gdi32.lib user32.lib ole32.lib shlwapi.lib ^
   /out:AnyClaw.exe
```

## 安装

1. 从 [Releases](https://github.com/aetheros/anyclaw/releases) 下载最新版本
2. 解压到任意目录
3. 运行 `AnyClaw.exe`
4. 首次运行将显示配置向导

## 配置

配置文件位于 `%APPDATA%\AnyClaw\config.json`：

```json
{
  "gateway_port": 3578,
  "poll_interval_sec": 10,
  "theme": "dark",
  "language": "zh-CN",
  "auto_start": false,
  "minimize_to_tray": true,
  "show_notifications": true
}
```

## 项目结构

```
AnyClaw/
├── src/
│   ├── main.cpp          # WinMain 入口
│   ├── tray.cpp          # 系统托盘实现
│   ├── settings.cpp      # 配置持久化
│   ├── resources.rc      # Windows 资源文件
│   ├── tray.h            # 托盘接口定义
│   ├── gui.h             # GUI 窗口接口
│   ├── settings.h        # 配置结构定义
│   ├── health.h          # 健康监控接口
│   └── openclaw_mgr.h    # OpenClaw 管理接口
├── thirdparty/
│   ├── imgui/            # Dear ImGui 源码
│   └── glfw/             # GLFW 头文件
├── CMakeLists.txt
└── README.md
```

## 技术栈

- **语言:** C++17
- **GUI:** Dear ImGui + OpenGL 3.3 + GLFW 3
- **系统:** Win32 API (Shell, Registry, GDI)
- **网络:** WinHTTP (健康检查)
- **构建:** CMake / Visual Studio

## License

<!-- License 占位 -->
MIT License — 详见 [LICENSE](LICENSE) 文件。

---

**AnyClaw** — Part of the [OpenClaw](https://github.com/aetheros/openclaw) ecosystem.
