# VS Code Configuration for QtNetworkRequest

## 环境配置

### 方法一：使用环境变量（推荐）

1. **设置 Qt 环境变量**：
   ```cmd
   set QT_DIR=C:\Qt\Qt5.6.3\5.6.3\msvc2015_64
   ```

2. **或者设置 QTDIR**（兼容性）：
   ```cmd
   set QTDIR=C:\Qt\Qt5.6.3\5.6.3\msvc2015_64
   ```

3. **Visual Studio 会自动检测**，无需额外设置

### 方法二：使用配置脚本

运行项目根目录的配置脚本：
```cmd
setup_vscode.bat "C:\Qt\Qt5.6.3\5.6.3\msvc2015_64"
```

### 方法三：手动设置

#### 设置系统环境变量
1. 打开 "系统属性" → "高级" → "环境变量"
2. 添加系统变量：
   - `QT_DIR`: `C:\Qt\Qt5.6.3\5.6.3\msvc2015_64`
   - `QTDIR`: `C:\Qt\Qt5.6.3\5.6.3\msvc2015_64`（可选）

#### 在 VS Code 中设置
1. 按 `Ctrl+Shift+P`
2. 选择 "Preferences: Open Settings (JSON)"
3. 添加以下配置：
   ```json
   {
       "cmake.configureOnOpen": true,
       "cmake.buildDirectory": "${workspaceFolder}/build",
       "qt.qt5Path": "${env:QT_DIR}"
   }
   ```

## 配置文件说明

### c_cpp_properties.json
- 使用 `${env:QT_DIR}` 引用 Qt 路径
- 使用 `${env:VCToolsInstallDir}` 自动检测 VS 编译器路径
- 支持多种 Qt 模块（QtCore, QtWidgets, QtNetwork, QtXml）

### launch.json
- 调试配置自动使用 `${env:QT_DIR}/bin` 路径
- 支持 NetworkRequestTool 和 QtDownloader 的调试

### tasks.json
- 使用 CMake 插件进行构建
- 支持并行编译

## 故障排除

### 1. Qt 路径问题
如果出现 "Cannot find Qt" 错误：
```cmd
echo %QT_DIR%
# 检查路径是否正确
dir "%QT_DIR%"
```

### 2. 编译器路径问题
如果出现 "Cannot find compiler" 错误：
```cmd
echo %VCToolsInstallDir%
# 检查 VS 安装
dir "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\"
```

### 3. 重新加载配置
修改环境变量后，重启 VS Code 或运行：
```cmd
code --reload-window
```

## 支持的 Qt 版本
- Qt 5.6.3+
- Qt 5.12.x
- Qt 5.15.x
- Qt 6.x（需要适当调整）

## 支持的 Visual Studio 版本
- Visual Studio 2022
- Visual Studio 2019
- Visual Studio 2017