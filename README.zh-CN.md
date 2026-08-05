# QtMultiThreadNetwork

> **[English](#) | 中文** — 完整中英双语版请查看 [README.md](README.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://www.qt.io)
[![Qt Version](https://img.shields.io/badge/Qt-5.6.3%2B-green.svg)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Build Status](https://github.com/lucaswang420/qt-network-request/actions/workflows/build.yml/badge.svg)](https://github.com/lucaswang420/qt-network-request/actions)

基于 Qt Network 模块构建的高性能、线程安全的 C++ 多线程 HTTP(S)/FTP 网络请求库。

## 目录

- [功能特性](#功能特性)
- [环境要求](#环境要求)
- [安装](#安装)
- [快速开始](#快速开始)
- [使用示例](#使用示例)
- [API 参考](#api-参考)
- [构建](#构建)
- [测试](#测试)
- [贡献](#贡献)
- [许可证](#许可证)
- [更新日志](#更新日志)

## 功能特性

### 🚀 核心能力

- **多线程架构**：每个请求在独立线程中执行，使用托管的线程池
- **并发操作**：支持单请求和批量请求两种模式
- **多线程下载**：大文件多通道并行下载，更高性能（threadCount=0 时自动检测 CPU 核心数）
- **协议支持**：HTTP(S)/FTP，完整请求方法（GET/POST/PUT/DELETE/HEAD/PATCH/OPTIONS）
- **异步 API**：非阻塞操作，通过信号/槽机制推送进度
- **线程安全**：所有公共方法均线程安全，使用原子操作

### 🛠️ 高级特性

- **内存映射文件**：利用平台特定 API 实现大文件高效 I/O
- **批量操作**：多请求编组，聚合进度追踪
- **指数退避重试**：通过可插拔策略自动重试瞬时故障（连接拒绝、超时、SSL 错误）
- **可扩展架构**：模板方法 + 策略 + 状态模式提供清晰的扩展点；自注册工厂消除新增请求类型时的 switch-case
- **代理支持**：支持逐请求代理和全局代理，两级级联
- **持久化 Cookie Jar**：基于文件的 Cookie 存储，跨请求共享
- **请求优先级队列**：线程饱和时高优先级请求优先执行
- **进度追踪**：下载、上传、批量操作的实时进度更新
- **跨平台**：Windows、Linux、macOS 全支持，含平台特定优化
- **OAuth2 认证**：Client Credentials / Password / Refresh Token，异步 token 获取，线程安全内存缓存，401 自动续期 (v2.3)
- **环境变量**：多命名环境（dev/staging/prod），`{{var}}` 全局替换（URL/Headers/Body/Auth），GUI 下拉切换 + 管理对话框 (v2.3)
- **响应增强**：增量搜索（高亮导航）、一键复制、保存到文件、JSON 格式化/原始切换 (v2.3)
- **请求收藏**：左侧树形面板，文件夹/请求管理，持久化到 collection.json，点击加载 (v2.3)
- **Postman v2.1 支持**：双向导入导出（文件夹、请求、认证字段完整往返）(v2.3)

### 📦 示例应用

- **QtRequester**：HTTP 请求测试 GUI 演示程序（位于 `samples/networkrequesttool/`）
- **QtDownloader**：支持智能多线程的下载管理器（位于 `samples/networkdownloader/`）
- **Unit Tests**：覆盖所有功能的全面测试套件

## 环境要求

### 构建要求

- **C++17 兼容编译器**（MSVC 2017+、GCC 7+、Clang 6+）
- **Qt 5.6.x+**（需 Core、Network、Widgets、Xml、Test 模块）
- **CMake 3.15+**（推荐）或 QMake
- **OpenSSL 1.1.1**（用于 HTTPS 支持）

### 平台特定要求

#### Windows

- Visual Studio 2017+ 或 MSVC 构建工具
- Windows SDK
- OpenSSL DLL（已包含在 ThirdParty/ 目录）

#### Linux

- GCC 7+ 或 Clang 6+
- OpenSSL 开发包：`libssl-dev`、`libcrypto-dev`

#### macOS

- Xcode 10+（Clang）
- 通过 Homebrew 或系统包安装 OpenSSL

## 安装

### 使用 CMake（推荐）

```bash
# 克隆仓库
git clone https://github.com/lucaswang420/qt-network-request.git
cd qt-network-request

# 配置并构建（Windows）
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# 配置并构建（Linux）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# 配置并构建（macOS）
# （需要 Homebrew 安装的 OpenSSL 1.1）
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
# 注意：如果在 Apple Silicon（M1/M2）上使用 Qt 5.x x86_64 二进制包，需追加：-DCMAKE_OSX_ARCHITECTURES="x86_64"
cmake --build build --config Release --parallel

# 安装（可选）
cmake --install build --prefix /usr/local
```

### 使用 QMake

```bash
# 生成 Visual Studio 解决方案
qmake -r -tp vc QtNetworkRequest.pro

# 使用 nmake 构建
qmake
nmake release
```

### 使用提供的脚本

#### Windows

```bash
# Windows 构建脚本
scripts\build_win.bat

# 生成 Visual Studio 解决方案
GenerateVsSln.bat
```

#### Linux

```bash
# Linux 构建脚本（需要执行权限）
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh

# 带调试符号构建
./scripts/build_linux.sh --debug

# 清理构建并运行测试
./scripts/build_linux.sh --clean --tests
```

#### macOS

```bash
# 使用提供的脚本构建（在 scripts/ 目录下运行）
./build_macos.sh --release

# 带测试和指定架构
./build_macos.sh --release --tests --arch x86_64

# 或直接通过 CMake 构建
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
cmake --build build --config Release --parallel
```

## 快速开始

### 基本用法

```cpp
#include "requestcontext.h"
#include "networkrequestmanager.h"
#include "networkreply.h"

using namespace QtNetworkRequest;

// 在主线程初始化
NetworkRequestManager::initialize();

// 通过流式构造器构建多线程下载请求
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://example.com/file.zip")
        .type(RequestType::MTDownload)
        .showProgress(true)
        .downloadConfig(std::make_unique<DownloadConfig>(
            DownloadConfig{ .saveDir = "downloads", .overwriteFile = true, .threadCount = 0 }))
        .build());
/*
 * 线程数选项：
 * - 0：自动检测 CPU 核心数（推荐，适用于大多数场景）
 * - 1：单线程下载
 * - N>1：使用 N 个线程进行多线程下载
 */

if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onFinished);
}

// 退出前清理
NetworkRequestManager::unInitialize();
```

### 批量操作

```cpp
// 使用流式构造器准备批量请求
QtNetworkRequest::BatchRequestPtrTasks tasks;
for (const QString& url : urls) {
    tasks.push_back(
        RequestContextBuilder()
            .url(url)
            .type(RequestType::Download)
            .downloadConfig(std::make_unique<DownloadConfig>(
                DownloadConfig{ .saveDir = "downloads" }))
            .build());
}

// 执行批量请求并追踪进度
quint64 batchId = 0;
auto reply = NetworkRequestManager::globalInstance()->postBatchRequest(std::move(tasks), batchId);
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onBatchFinished);
}
```

## 使用示例

### 界面预览

Network Request Tool 演示

![Network Request Tool](./images/request_tool.png)

MultiThread Downloader 演示

![MultiThread Downloader](./images/downloader.png)

### 文件下载

```cpp
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/image/png")
        .type(RequestType::Download)
        .showProgress(true)
        .downloadConfig(std::make_unique<DownloadConfig>(
            DownloadConfig{ .saveDir = "Download", .overwriteFile = true }))
        .build());
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onDownloadFinished);
}
```

### 文件上传

```cpp
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/post")
        .type(RequestType::Upload)
        .showProgress(true)
        .uploadConfig(std::make_unique<UploadConfig>(
            UploadConfig{ .filePath = "resources/1.png", .usePutMethod = false }))
        .build());
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onUploadFinished);
}
```

### HTTP GET 请求

```cpp
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/get?userId=123&userName=456")
        .type(RequestType::Get)
        .retry(3)          // retryOnFailed = true，最大重试 3 次，间隔 1s
        .maxRedirects(3)
        .build());
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onGetFinished);
}
```

### HTTP POST 请求

```cpp
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/post")
        .type(RequestType::Post)
        .body("userId=123&userName=456")
        .retry(3)
        .maxRedirects(3)
        .build());
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onPostFinished);
}
```

### 代理配置

```cpp
// 全局代理（自动应用于所有请求，除非单独覆盖）
ProxyConfig proxy;
proxy.enabled = true;
proxy.host = "proxy.example.com";
proxy.port = 3128;
proxy.user = "username";
proxy.password = "password";
NetworkRequestManager::setGlobalProxy(proxy);

// 逐请求代理（覆盖全局设置）——传入堆分配的 ProxyConfig
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/get")
        .type(RequestType::Get)
        .proxyConfig(std::make_unique<ProxyConfig>(
            ProxyConfig{ .enabled = true, .host = "127.0.0.1", .port = 8080 }))
        .build());
```

### SSL/TLS 配置

```cpp
// 全局安全默认值自动生效（VerifyPeer + TLS1.2+ + 系统 CA）。
// 可通过构造器逐请求覆盖：
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://self-signed.example.com/api")
        .type(RequestType::Get)
        .sslConfig(SslConfig::secureDefault())   // 拷贝；也可先构造自定义 SslConfig
        .build());
```

### 持久化 Cookie Jar

```cpp
// 启用基于文件的 Cookie 存储（在主线程中、发起请求之前调用）
NetworkRequestManager::setCookieStoragePath("cookies.json");

// Cookie 在每次响应后自动保存，
// 并在下次应用启动时重新加载

// 禁用（仅内存模式）
NetworkRequestManager::setCookieStoragePath(QString());
```

### 请求优先级

```cpp
auto highReply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/get")
        .type(RequestType::Get)
        .priority(10)      // 数值越大越优先
        .build());

auto lowReply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/get")
        .type(RequestType::Get)
        .priority(0)       // 默认优先级
        .build());

// 线程饱和时，优先级 10 的请求会先于优先级 0 执行
```

### 失败重试

```cpp
auto reply = NetworkRequestManager::globalInstance()->postRequest(
    RequestContextBuilder()
        .url("https://httpbin.org/get")
        .type(RequestType::Get)
        .retry(3, 1000)    // 最多重试 3 次，基础延迟 1s（随后 2s、4s——指数增长）
        .build());
```

### 请求管理

```cpp
// 停止单个请求
quint64 taskId = 1;
NetworkRequestManager::globalInstance()->stopRequest(taskId);

// 停止批量请求
quint64 batchId = 1;
NetworkRequestManager::globalInstance()->stopBatchRequests(batchId);

// 停止所有请求
NetworkRequestManager::globalInstance()->stopAllRequest();
```

## API 参考

### 核心类

#### NetworkRequestManager

管理线程池和请求生命周期的单例类。

**主要方法：**

- `initialize()`：初始化管理器（必须在主线程调用）
- `unInitialize()`：清理资源（必须在主线程调用）
- `postRequest(std::unique_ptr<RequestContext>)`：通过 RequestContextBuilder 构建并执行单个请求
- `postBatchRequest(BatchRequestPtrTasks)`：执行批量请求
- `stopRequest(quint64)`：停止指定请求
- `stopBatchRequests(quint64)`：停止批量请求
- `stopAllRequest()`：停止所有活动请求
- `setGlobalProxy(ProxyConfig)`：设置全局代理（应用于所有请求）
- `setCookieStoragePath(path)`：通过文件路径启用持久化 Cookie jar
- `setMaxThreadCount(int)`：设置线程池大小（1-100）

**信号：**

- `downloadProgress(quint64, qint64, qint64)`：单个请求的下载进度
- `uploadProgress(quint64, qint64, qint64)`：单个请求的上传进度
- `batchDownloadProgress(quint64, qint64)`：批量请求的聚合下载进度
- `batchUploadProgress(quint64, qint64)`：批量请求的聚合上传进度

#### RequestContext

网络请求的配置结构体（替代旧的 RequestTask）。

**主要属性：**

- `url`：目标 URL
- `type`：请求类型（Download、Upload、Get、Post、Put、Delete、Head）
- `headers`：请求头（QMap<QByteArray, QByteArray>）
- `body`：POST/PUT 的请求体
- `behavior.showProgress`：是否启用进度上报
- `behavior.retryOnFailed`：是否在瞬时故障时启用重试
- `behavior.maxRetryCount`：最大重试次数（默认：3）
- `behavior.retryDelayMs`：基础重试延迟（毫秒，每次翻倍，默认：1000）
- `behavior.maxRedirectionCount`：最大重定向限制（默认：3）
- `behavior.priority`：请求优先级（数值越大越优先，默认：0）
- `behavior.transferTimeout`：传输超时（毫秒，默认：30000）
- `proxyConfig`：逐请求代理覆盖（覆盖全局代理）
- `downloadConfig`：下载配置（saveDir、overwriteFile、threadCount）
- `uploadConfig`：上传配置（filePath、usePutMethod、useFormData、useStream）
- `userContext`：用户自定义上下文数据

#### NetworkReply

处理单个请求或批量请求的异步响应。

**信号：**

- `requestFinished(QSharedPointer<ResponseResult>)`：请求完成后发出（无论成功或出错）

#### ResponseResult

包含请求响应数据的结构体。

**主要属性：**

- `success`：请求是否成功
- `cancelled`：请求是否被取消
- `errorMessage`：失败时的错误信息
- `body`：响应体数据
- `headers`：响应头
- `performance.durationMs`：请求耗时（毫秒）
- `performance.bytesReceived`：接收字节数
- `performance.bytesSent`：发送字节数
- `userContext`：用户自定义上下文数据

#### DownloadConfig

下载操作的配置结构体。

**主要属性：**

- `saveFileName`：下载文件的自定义文件名
- `saveDir`：下载文件保存目录
- `overwriteFile`：是否覆盖已存在文件（默认：false）
- `threadCount`：多线程下载的线程数（默认：0 = 自动检测 CPU 核心数）

#### UploadConfig

上传操作的配置结构体。

**主要属性：**

- `filePath`：待上传文件路径
- `data`：待上传的原始数据
- `usePutMethod`：使用 HTTP PUT 方法而非 POST（默认：false）
- `useStream`：使用流式上传（默认：false）
- `useFormData`：使用 multipart 表单数据（默认：false）

## 构建

### 构建配置

本库支持 CMake 和 QMake 两种构建系统，采用标准化命名：

- **CMake**：现代跨平台构建系统，自动检测依赖
- **QMake**：传统 Qt 构建系统，支持 Visual Studio 集成

### 构建目标

**CMake 目标：**

- `QNetworkRequest`：核心库（DLL）
- `QtRequester`：GUI 演示应用（源码位于 `samples/networkrequesttool/`）
- `QtDownloader`：下载管理器应用（源码位于 `samples/networkdownloader/`）
- `UnitTests`：测试套件

**QMake 目标：**

- `QNetworkRequest`：核心库（DLL）
- `QtRequester`：GUI 演示应用（源码位于 `samples/networkrequesttool/`）
- `QtDownloader`：下载管理器应用（源码位于 `samples/networkdownloader/`）

### 构建类型

```bash
# CMake Release 构建
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# CMake Debug 构建
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# QMake 构建
qmake -r -tp vc QtNetworkRequest.pro
nmake release
nmake debug
```

### 构建脚本

```bash
# Windows 构建脚本
scripts\build_win.bat

# 生成 Visual Studio 解决方案
GenerateVsSln.bat
```

### 跨平台构建

#### 直接使用 CMake

```bash
# Linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# macOS
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
cmake --build build --config Release

# Windows
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### 使用构建脚本

```bash
# Windows（批处理脚本）
scripts\build_win.bat

# Linux（Shell 脚本）
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh --release
```

## 跨平台兼容性

QtMultiThreadNetwork 设计为跨平台运行，通过适当的构建配置即可。

### 支持平台

| 平台 | 状态 | 备注 |
|------|------|------|
| **Windows** | ✅ 完全支持 | 主要开发平台，MSVC 2017+ |
| **Linux** | ✅ 完全支持 | GCC 7+ 或 Clang 6+，CMake 3.15+ |
| **macOS** | ✅ 完全支持 | Xcode 10+（Clang），CMake 3.15+ |

### 组件兼容性

| 组件 | Windows | Linux | macOS | 状态 |
|------|---------|-------|-------|------|
| 核心网络功能 | ✅ | ✅ | ✅ | 基于 Qt Network（跨平台） |
| 内存映射文件 | ✅ | ✅ | ✅ | 使用平台特定 API 实现 |
| 多线程下载 | ✅ | ✅ | ✅ | 跨平台兼容 |
| 线程池管理 | ✅ | ✅ | ✅ | 使用 Qt 线程框架 |
| 构建系统 | ✅ | ✅ | ✅ | CMake + 平台特定优化 |
| OpenSSL 集成 | DLL | 动态 | 动态 | 平台特定链接方式 |
| GUI 应用 | ✅ | ✅ | ✅ | 条件 WIN32 标志，正确生成 GUI 应用 |

### 平台特定实现

#### 内存映射文件

- **Windows**：使用 `CreateFileMapping` / `MapViewOfFile`
- **Linux/macOS**：使用 `mmap` / `munmap`
- **文件大小**：所有平台均支持大文件（>4GB）

#### 构建要求

##### Windows

- **编译器**：MSVC 2017+
- **Qt**：5.6.x+
- **OpenSSL**：DLL 文件（libeay32.dll、ssleay32.dll）

##### Linux

- **编译器**：GCC 7+ 或 Clang 6+
- **Qt**：5.6.x+
- **OpenSSL**：开发包（libssl-dev、libcrypto-dev）
- **构建**：CMake 3.15+

##### macOS

- **编译器**：Xcode 10+（Clang）
- **Qt**：5.6.x+
- **OpenSSL**：Homebrew 安装（`brew install openssl@1.1`）
- **构建**：CMake 3.15+
- **架构**：支持 Intel（`x86_64`）和 Apple Silicon（`arm64`）。如果架构与 Qt 二进制包不同，请使用 `-DCMAKE_OSX_ARCHITECTURES` 指定目标架构。

### 平台特定注意事项

1. **OpenSSL 处理**：平台特定的动态库链接（Windows DLL、Unix 动态库）
2. **构建系统**：CMake 自动检测并配置平台特定设置

### 跨平台构建说明

```bash
# Linux/macOS 构建
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 在 Unix 系统上自动查找并链接 OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(QNetworkRequest PRIVATE OpenSSL::SSL OpenSSL::Crypto)
```

### 迁移指南

构建系统现已支持跨平台编译，具备自动平台检测功能：

1. **自动 GUI 标志**：WIN32 标志仅在 Windows 上自动应用于 GUI 可执行文件
2. **OpenSSL 检测**：CMake 在所有平台上自动查找并链接 OpenSSL
3. **跨平台路径**：CMake 处理平台特定的路径分隔符
4. **线程**：线程池管理在所有平台上行为一致

核心库功能与平台无关，在正确的构建配置下可在所有支持的平台上无缝运行。

## 测试

### 运行测试

```bash
# 使用 CTest
cd build
ctest -C Release

# 直接执行
./test/Release/UnitTests.exe
```

### 测试覆盖

测试套件覆盖以下功能：

- 基本请求功能（GET、POST、PUT、DELETE、HEAD、表单数据）
- 文件下载（单线程和多线程）
- 文件上传（PUT 方式上传文件）
- 代理配置（全局和逐请求代理）
- 重试行为（瞬时故障重试、禁用重试）
- 持久化 Cookie jar（保存、重新加载、跨请求 Cookie 共享）
- 请求优先级队列（线程池饱和时的优先级排序）
- 请求取消（停止正在运行的请求、停止批量、停止全部）
- 快速取消压力测试（20 次创建/取消循环）
- 错误处理场景
- 进度上报
- 线程安全
- 内存管理
- 平台特定实现

## 贡献

欢迎贡献！请遵循以下指引：

1. **Fork 仓库**并创建功能分支
2. **遵循现有代码风格**和模式
3. **为新功能添加测试**
4. **根据需要更新文档**
5. **提交 Pull Request**，并附上清晰的描述

### 开发环境搭建

```bash
# 克隆（含子模块）
git clone --recursive https://github.com/lucaswang420/qt-network-request.git
cd qt-network-request

# 搭建开发构建
cmake -S . -B build-dev -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-dev --config Debug
```

### 代码风格

- 使用 C++17 特性和现代 C++ 实践
- 遵循 Qt 编码规范
- 使用智能指针管理内存
- 实现合理的错误处理
- 添加完善的文档

## 许可证

本项目采用 MIT 许可证 — 详见 [LICENSE](LICENSE) 文件。

**Copyright (c) 2025-2026 Lucas Wang。基于 MIT 许可证授权。**
