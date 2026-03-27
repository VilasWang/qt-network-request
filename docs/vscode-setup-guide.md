# VSCode 编译和调试配置指南

本指南介绍如何在 Visual Studio Code 中配置和使用 Qt Network Request 项目的编译和调试功能。

## 📋 目录

- [环境准备](#环境准备)
- [VSCode 配置](#vscode-配置)
- [编译项目](#编译项目)
- [调试项目](#调试项目)
- [常见问题排查](#常见问题排查)
- [高级配置](#高级配置)

## 🚀 环境准备

### 1. 必需软件

- **Visual Studio Code**：最新版本
- **CMake Tools 扩展**：在VSCode扩展市场搜索并安装 "CMake Tools"
- **C/C++ 扩展**：安装 "C/C++" (由 Microsoft 提供)
- **Visual Studio**：安装了C++构建工具的 Visual Studio 2019/2022
- **Qt**：Qt 5.6 或更高版本

### 2. 环境变量设置

#### 设置 QT_DIR 环境变量

**临时设置（当前终端会话）：**
```bash
# Windows CMD
set QT_DIR=C:\Qt\5.15.2\msvc2019_64

# Windows PowerShell
$env:QT_DIR="C:\Qt\5.15.2\msvc2019_64"
```

**永久设置（推荐）：**
```bash
# 使用 setx 命令（需要管理员权限）
setx QT_DIR "C:\Qt\5.15.2\msvc2019_64"

# 或者通过系统设置：
# 1. 右键"此电脑" → "属性" → "高级系统设置"
# 2. 点击"环境变量"
# 3. 在"用户变量"中新建 QT_DIR，值为Qt安装路径
```

#### 验证环境变量

```bash
# 检查 QT_DIR 是否设置成功
echo %QT_DIR%

# 应该输出类似：C:\Qt\5.15.2\msvc2019_64
```

#### 可选：使用 setup_vscode.bat 脚本

项目提供了自动化配置脚本：

```bash
# 直接运行（使用环境变量中的QT_DIR）
.vscode\setup_vscode.bat

# 或指定Qt路径
.vscode\setup_vscode.bat "C:\Qt\5.15.2\msvc2019_64"
```

## ⚙️ VSCode 配置

### 1. 扩展安装

打开VSCode，安装以下扩展：

1. **C/C++** (ms-vscode.cpptools)
2. **CMake Tools** (ms-vscode.cmake-tools)

### 2. 项目配置文件说明

项目包含以下配置文件：

- `.vscode/settings.json` - VSCode工作区设置
- `.vscode/launch.json` - 调试配置
- `.vscode/tasks.json` - 构建任务配置
- `.vscode/c_cpp_properties.json` - C++ IntelliSense配置

### 3. 配置 IntelliSense

配置文件自动使用了以下环境变量：
- `${env:QT_DIR}` - Qt安装路径
- `${env:VCToolsInstallDir}` - Visual C++工具路径
- `${env:WindowsSdkDir}` - Windows SDK路径

如果IntelliSense不工作：

1. 按 `Ctrl+Shift+P`
2. 输入 "C/C++: Reset IntelliSense Database"
3. 选择重置并等待完成

## 🔨 编译项目

### 方法一：使用 CMake Tools（推荐）

#### 1. 配置 CMake

- **打开命令面板：** `Ctrl+Shift+P`
- **输入：** `CMake: Configure`
- **选择编译套件：** 选择 "Visual Studio Community 2019 Release - amd64" 或类似选项
- **选择构建类型：** 选择 `Debug` 或 `Release`

#### 2. 构建项目

**方式A：使用命令面板**
```bash
Ctrl+Shift+P → "CMake: Build"
```

**方式B：使用状态栏**
- 点击VSCode底部状态栏的 "Build" 按钮

**方式C：使用快捷键**
- 按 `F7`（如果已配置CMake Tools快捷键）

#### 3. 查看构建输出

- 构建进度和错误会显示在 "Output" 面板的 "CMake" 标签页中
- 成功后，可执行文件会生成在 `build/Debug/` 或 `build/Release/` 目录

### 方法二：使用任务（Tasks）

1. 按 `Ctrl+Shift+B`
2. 选择 "cmake: build"

## 🐛 调试项目

### 1. 选择调试目标

1. **打开CMake面板：**
   - 点击左侧活动栏的 "CMake" 图标（火焰图标）
   - 或按 `Ctrl+Shift+P` → "CMake: Set Debug Target"

2. **选择目标：**
   - `NetworkRequestTool.exe` - 网络请求工具
   - `QtDownloader.exe` - 下载器示例

### 2. 启动调试

**方式A：使用调试面板**
1. 点击左侧活动栏的 "Run and Debug" 图标（或按 `Ctrl+Shift+D`）
2. 在顶部下拉菜单选择调试配置：
   - "Debug request tool (MSVC)" 或
   - "Debug downloader (MSVC)"
3. 点击绿色播放按钮（或按 `F5`）

**方式B：直接启动**
- 按 `F5`，如果已经选择过调试配置

### 3. 调试功能

- **设置断点：** 点击代码行号左侧
- **条件断点：** 右键点击断点图标 → "Edit Breakpoint" → "Add Condition"
- **查看变量：** 鼠标悬停在变量上或查看 "VARIABLES" 面板
- **监视表达式：** 在 "WATCH" 面板添加表达式
- **调用堆栈：** 查看 "CALL STACK" 面板

### 4. 调试配置说明

launch.json 包含两个调试配置：

1. **Debug request tool (MSVC)** - 调试 NetworkRequestTool
2. **Debug downloader (MSVC)** - 调试 QtDownloader

两个配置都会：
- 自动使用CMake选中的调试目标
- 添加Qt的bin目录到PATH，确保Qt DLL正确加载
- 启用详细日志记录

## 🔧 常见问题排查

### 问题1：CMake 配置失败

**症状：** 配置时提示找不到编译器或Qt

**解决方案：**
```bash
# 1. 检查Visual Studio是否正确安装
dir "C:\Program Files (x86)\Microsoft Visual Studio"

# 2. 检查Qt环境变量
echo %QT_DIR%

# 3. 手动指定编译套件
# Ctrl+Shift+P → "CMake: Scan for kits"
# 然后重新配置
```

### 问题2：找不到 Qt 头文件

**症状：** IntelliSense 提示找不到 Qt 头文件

**解决方案：**
1. 检查 `.vscode/c_cpp_properties.json` 中的 includePath 配置
2. 确保 `QT_DIR` 环境变量已正确设置
3. 重置 IntelliSense：`Ctrl+Shift+P` → "C/C++: Reset IntelliSense Database"

### 问题3：调试时找不到 DLL

**症状：** 启动调试时提示缺少 Qt DLL 文件

**解决方案：**
1. 检查 launch.json 中的 PATH 环境变量配置：
   ```json
   "environment": [
       {
           "name": "PATH",
           "value": "${env:PATH};${env:QT_DIR}/bin"
       }
   ]
   ```
2. 确保 `%QT_DIR%/bin` 目录存在且包含 Qt DLL
3. 将 `%QT_DIR%/bin` 添加到系统PATH环境变量

### 问题4：构建输出乱码

**症状：** 构建输出中的中文显示为乱码

**解决方案：**
在 VSCode 设置中添加：
```json
{
    "terminal.integrated.encoding": "gbk",
    "files.encoding": "utf8"
}
```

### 问题5：CMake 找不到 Ninja

**症状：** 配置时提示找不到 ninja.exe

**解决方案：**
项目已移除 Ninja 硬编码要求，CMake 会自动选择合适的生成器：
- Visual Studio 项目生成器（推荐）
- 或 MinGW Makefiles（如果使用 MinGW）

### 问题6：断点不命中

**症状：** 设置的断点不被触发

**解决方案：**
1. 确保使用的是 Debug 构建配置
2. 检查代码优化设置：确保 CMAKE_BUILD_TYPE=Debug
3. 尝试清理并重新构建：
   ```bash
   Ctrl+Shift+P → "CMake: Clean"
   Ctrl+Shift+P → "CMake: Build"
   ```

## 🎯 高级配置

### 1. 多版本Qt切换

如果需要在不同Qt版本间切换：

**方法A：临时切换**
```bash
# 在当前终端设置不同的QT_DIR
set QT_DIR=C:\Qt\5.15.2\msvc2019_64
# 然后重新配置CMake
```

**方法B：使用CMake变量**
```bash
# 在CMake配置时指定Qt路径
Ctrl+Shift+P → "CMake: Configure with options"
# 添加: -DCMAKE_PREFIX_PATH=C:/Qt/5.12.2/msvc2019_64
```

### 2. 自定义构建选项

在 `.vscode/settings.json` 中添加CMake配置：

```json
{
    "cmake.configureArgs": [
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DQt5_DIR=C:/Qt/5.12.2/msvc2019_64/lib/cmake/Qt5"
    ],
    "cmake.buildArgs": [
        "--parallel", "4"
    ]
}
```

### 3. 添加新的调试配置

如需添加新的调试目标，在 `.vscode/launch.json` 中添加：

```json
{
    "name": "Debug MyTarget (MSVC)",
    "type": "cppvsdbg",
    "request": "launch",
    "program": "${command:cmake.launchTargetPath}",
    "args": [],
    "stopAtEntry": false,
    "cwd": "${workspaceFolder}",
    "environment": [
        {
            "name": "PATH",
            "value": "${env:PATH};${env:QT_DIR}/bin"
        }
    ],
    "console": "internalConsole"
}
```

### 4. 性能分析

使用Visual Studio的性能分析工具：

1. 以调试模式启动程序
2. 在VSCode中使用 "Performance" 功能（需要额外扩展）
3. 或使用Visual Studio standalone profiling工具

## 📚 相关文档

- [Qt 官方文档](https://doc.qt.io/)
- [CMake Tools 文档](https://vector-of-bool.github.io/docs/vscode-cmake-tools/)
- [VSCode 调试文档](https://code.visualstudio.com/docs/editor/debugging)

## 🆘 获取帮助

如果遇到本指南未涵盖的问题：

1. 查看 VSCode 的 "Output" 面板，选择相应标签页查看详细日志
2. 检查 `build/CMakeFiles/CMakeOutput.log` 和 `CMakeError.log`
3. 在项目仓库提交 Issue

## 📝 快速参考

### 常用快捷键

| 操作 | 快捷键 |
|------|--------|
| CMake 配置 | `Ctrl+Shift+P` → "CMake: Configure" |
| 构建 | `F7` 或点击状态栏 "Build" |
| 调试 | `F5` |
| 设置断点 | `F9` |
| 单步跳过 | `F10` |
| 单步进入 | `F11` |
| 继续运行 | `F5` |
| 停止调试 | `Shift+F5` |

### 常用命令

```bash
# 清理构建
Ctrl+Shift+P → "CMake: Clean"

# 选择调试目标
Ctrl+Shift+P → "CMake: Set Debug Target"

# 扫描编译套件
Ctrl+Shift+P → "CMake: Scan for kits"

# 重置 IntelliSense
Ctrl+Shift+P → "C/C++: Reset IntelliSense Database"
```

---

**最后更新：** 2026-03-27
**维护者：** Qt Network Request 项目团队
