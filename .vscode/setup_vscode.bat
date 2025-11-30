@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo QtNetworkRequest VS Code 环境配置脚本
echo ========================================
echo.

:: Check for Qt environment variables
if not defined QT_DIR (
    if not defined QTDIR (
        echo Error: Qt environment variable not set!
        echo Please set QT_DIR or QTDIR environment variable to point to your Qt installation.
        echo Example: set QT_DIR=C:\Qt\Qt5.6.3\5.6.3\msvc2015_64
        echo.
        echo 或者运行此脚本时指定 Qt 路径:
        echo setup_vscode.bat "C:\Qt\Qt5.6.3\5.6.3\msvc2015_64"
        pause
        exit /b 1
    ) else (
        set QT_DIR=%QTDIR%
    )
)

:: If argument provided, use it as QT_DIR
if "%~1" neq "" (
    set QT_DIR=%~1
    echo Using provided Qt path: %QT_DIR%
)

:: Verify Qt directory exists
if not exist "%QT_DIR%" (
    echo Error: Qt directory not found: %QT_DIR%
    echo Please check your Qt installation path.
    pause
    exit /b 1
)

:: Create .vscode directory if it doesn't exist
if not exist .vscode mkdir .vscode

:: Create or update VS Code settings
echo {
    "cmake.configureOnOpen": true,
    "cmake.generator": "Ninja",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.buildArgs": [
        "--parallel"
    ],
    "cmake.configureArgs": [
        "-DCMAKE_BUILD_TYPE=Debug"
    ],
    "cmake.debugConfig": {
        "args": [],
        "cwd": "${workspaceFolder}"
    },
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "%QT_DIR%/bin/qmake.bat"
    },
    "qt.qmldir": "%QT_DIR%/qml",
    "qt.plugins.path": "%QT_DIR%/plugins",
    "qt.qt5Path": "%QT_DIR%"
} > .vscode\settings.json

echo.
echo ========================================
echo VS Code 配置已完成!
echo ========================================
echo.
echo 已设置以下环境变量:
echo - QT_DIR: %QT_DIR%
echo.
echo 请确保以下环境变量已正确设置:
echo 1. QT_DIR 或 QTDIR 指向 Qt 安装目录
echo 2. Visual Studio 已安装并可用
echo.
echo 重启 VS Code 使配置生效。
echo.
pause