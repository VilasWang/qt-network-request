# QtMultiThreadNetwork

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://www.qt.io)
[![Qt Version](https://img.shields.io/badge/Qt-5.6.3%2B-green.svg)](https://www.qt.io)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Build Status](https://github.com/lucaswang420/qt-network-request/actions/workflows/build.yml/badge.svg)](https://github.com/lucaswang420/qt-network-request/actions)

A high-performance, thread-safe C++ library that provides multi-threaded HTTP(S)/FTP networking capabilities built on top of Qt's Network module.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage Examples](#usage-examples)
- [API Reference](#api-reference)
- [Building](#building)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)
- [Changelog](#changelog)

## Features

### 🚀 Core Capabilities

- **Multi-threaded Architecture**: Each request executes in separate threads using a managed thread pool
- **Concurrent Operations**: Support for both single and batch request modes
- **Multi-threaded Downloads**: Large file downloads with multiple channels for faster performance (auto-detects CPU cores when threadCount=0)
- **Protocol Support**: HTTP(S)/FTP with full request method support (GET/POST/PUT/DELETE/HEAD)
- **Asynchronous API**: Non-blocking operations with signal/slot progress reporting
- **Thread Safety**: All public methods are thread-safe with atomic operations

### 🛠️ Advanced Features

- **Memory-Mapped Files**: Efficient file I/O for large downloads using platform-specific APIs
- **Batch Operations**: Group multiple requests with aggregated progress tracking
- **Retry with Exponential Backoff**: Automatic retry of transient failures (connection refused, timeout, SSL errors)
- **Proxy Support**: Per-request proxy or global proxy with two-level cascade
- **Persistent Cookie Jar**: File-backed cookie storage shared across all requests
- **Request Priority Queue**: Higher-priority requests execute first when threads are saturated
- **Progress Tracking**: Real-time progress updates for downloads, uploads, and batch operations
- **Cross-Platform**: Windows, Linux, and macOS support with platform-specific optimizations

### 📦 Sample Applications

- **QtRequester**: GUI demo application for testing HTTP requests (located in `samples/networkrequesttool/`)
- **QtDownloader**: Download manager with intelligent multi-threading support (located in `samples/networkdownloader/`)
- **Unit Tests**: Comprehensive test suite covering all functionality

## Requirements

### Build Requirements

- **C++17 compatible compiler** (MSVC 2017+, GCC 7+, Clang 6+)
- **Qt 5.6.x+** with Core, Network, Widgets, Xml, Test modules
- **CMake 3.15+** (recommended) or QMake
- **OpenSSL 1.1.1** for HTTPS support

### Platform-Specific Requirements

#### Windows

- Visual Studio 2017+ or MSVC build tools
- Windows SDK
- OpenSSL DLLs (included in ThirdParty/)

#### Linux

- GCC 7+ or Clang 6+
- OpenSSL development packages: `libssl-dev`, `libcrypto-dev`

#### macOS

- Xcode 10+ (Clang)
- OpenSSL via Homebrew or system packages

## Installation

### Using CMake (Recommended)

```bash
# Clone the repository
git clone https://github.com/lucaswang420/qt-network-request.git
cd qt-network-request

# Configure and build (Windows)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# Configure and build (Linux)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel

# Configure and build (macOS)
# (Requires Homebrew's OpenSSL 1.1)
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
# Note: If building on Apple Silicon (M1/M2) but using Qt 5.x x86_64 binaries, append: -DCMAKE_OSX_ARCHITECTURES="x86_64"
cmake --build build --config Release --parallel

# Install (optional)
cmake --install build --prefix /usr/local
```

### Using QMake

```bash
# Generate Visual Studio solution
qmake -r -tp vc QtNetworkRequest.pro

# Build using nmake
qmake
nmake release
```

### Using Provided Scripts

#### Windows

```bash
# Windows build script
scripts\build_win.bat

# Generate Visual Studio solution
GenerateVsSln.bat
```

#### Linux

```bash
# Linux build script (requires execution permissions)
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh

# Build with debug symbols
./scripts/build_linux.sh --debug

# Clean build and run tests
./scripts/build_linux.sh --clean --tests
```

#### macOS

```bash
# Build using the provided script (run from scripts/ directory)
./build_macos.sh --release

# With tests and explicit architecture
./build_macos.sh --release --tests --arch x86_64

# Or build directly via CMake
export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}"
cmake --build build --config Release --parallel
```

## Quick Start

### Basic Usage

```cpp
#include "networkrequestdefs.h"
#include "networkrequestmanager.h"
#include "networkreply.h"

// Initialize in main thread
NetworkRequestManager::initialize();

// Create request context
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://example.com/file.zip";
req->type = QtNetworkRequest::RequestType::MTDownload;
req->behavior.showProgress = true;

// Configure download settings
req->downloadConfig = std::make_unique<QtNetworkRequest::DownloadConfig>();
req->downloadConfig->saveDir = "downloads";
req->downloadConfig->overwriteFile = true;
req->downloadConfig->threadCount = 0; // 0 = auto detect CPU cores
/*
 * Thread count options:
 * - 0: Auto detect CPU cores (recommended for most cases)
 * - 1: Single-threaded download
 * - N>1: Use N threads for multi-threaded download
 */

// Execute asynchronously
auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onFinished);
}

// Cleanup before exit
NetworkRequestManager::unInitialize();
```

### Batch Operations

```cpp
// Prepare batch requests
QtNetworkRequest::BatchRequestPtrTasks tasks;
for (const QString& url : urls) {
    auto req = std::make_unique<QtNetworkRequest::RequestContext>();
    req->url = url;
    req->type = QtNetworkRequest::RequestType::Download;
    req->downloadConfig = std::make_unique<QtNetworkRequest::DownloadConfig>();
    req->downloadConfig->saveDir = "downloads";
    tasks.push_back(std::move(req));
}

// Execute batch with progress tracking
quint64 batchId = 0;
auto reply = NetworkRequestManager::globalInstance()->postBatchRequest(tasks, batchId);
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onBatchFinished);
}
```

## Usage Examples

### example

Network Request Tool demo
![Network Request Tool](./images/request_tool.png)

MultiThread Downloader demo
![MultiThread Downloader](./images/downloader.png)

### File Download

```cpp
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/image/png";
req->type = QtNetworkRequest::RequestType::Download;
req->behavior.showProgress = true;

req->downloadConfig = std::make_unique<QtNetworkRequest::DownloadConfig>();
req->downloadConfig->saveDir = "Download";
req->downloadConfig->overwriteFile = true;

auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onDownloadFinished);
}
```

### File Upload

```cpp
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/post";
req->type = QtNetworkRequest::RequestType::Upload;
req->behavior.showProgress = true;

req->uploadConfig = std::make_unique<QtNetworkRequest::UploadConfig>();
req->uploadConfig->filePath = "resources/1.png";
req->uploadConfig->usePutMethod = false;

auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onUploadFinished);
}
```

### HTTP GET Request

```cpp
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/get?userId=123&userName=456";
req->type = QtNetworkRequest::RequestType::Get;
req->behavior.retryOnFailed = true;
req->behavior.maxRedirectionCount = 3;

auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onGetFinished);
}
```

### HTTP POST Request

```cpp
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/post";
req->type = QtNetworkRequest::RequestType::Post;
req->body = "userId=123&userName=456";
req->behavior.retryOnFailed = true;
req->behavior.maxRedirectionCount = 3;

auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
if (reply) {
    connect(reply.get(), &NetworkReply::requestFinished, this, &MyClass::onPostFinished);
}
```

### Proxy Configuration

```cpp
// Global proxy (applied to all requests unless overridden)
QtNetworkRequest::ProxyConfig proxy;
proxy.enabled = true;
proxy.host = "proxy.example.com";
proxy.port = 3128;
proxy.user = "username";
proxy.password = "password";
NetworkRequestManager::setGlobalProxy(proxy);

// Per-request proxy (overrides global)
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/get";
req->type = QtNetworkRequest::RequestType::Get;
req->proxyConfig = std::make_unique<QtNetworkRequest::ProxyConfig>();
req->proxyConfig->enabled = true;
req->proxyConfig->host = "127.0.0.1";
req->proxyConfig->port = 8080;
```

### Persistent Cookie Jar

```cpp
// Enable file-backed cookie storage (in main thread before requests)
NetworkRequestManager::setCookieStoragePath("cookies.json");

// Cookies are automatically saved after each response
// and reloaded on next application start

// Disable (in-memory only)
NetworkRequestManager::setCookieStoragePath(QString());
```

### Request Priority

```cpp
auto highReq = std::make_unique<QtNetworkRequest::RequestContext>();
highReq->url = "https://httpbin.org/get";
highReq->type = QtNetworkRequest::RequestType::Get;
highReq->behavior.priority = 10; // higher = more urgent

auto lowReq = std::make_unique<QtNetworkRequest::RequestContext>();
lowReq->url = "https://httpbin.org/get";
lowReq->type = QtNetworkRequest::RequestType::Get;
lowReq->behavior.priority = 0; // default priority

// When threads are saturated, priority 10 executes before priority 0
```

### Retry on Failure

```cpp
auto req = std::make_unique<QtNetworkRequest::RequestContext>();
req->url = "https://httpbin.org/get";
req->type = QtNetworkRequest::RequestType::Get;
req->behavior.retryOnFailed = true;
req->behavior.maxRetryCount = 3;    // up to 3 retries
req->behavior.retryDelayMs = 1000;  // 1s, then 2s, then 4s (exponential)
```

### Request Management

```cpp
// Stop single request
quint64 taskId = 1;
NetworkRequestManager::globalInstance()->stopRequest(taskId);

// Stop batch requests
quint64 batchId = 1;
NetworkRequestManager::globalInstance()->stopBatchRequests(batchId);

// Stop all requests
NetworkRequestManager::globalInstance()->stopAllRequest();
```

## API Reference

### Core Classes

#### NetworkRequestManager

Singleton class that manages the thread pool and request lifecycle.

**Key Methods:**

- `initialize()`: Initialize the manager (must be called in main thread)
- `unInitialize()`: Cleanup resources (must be called in main thread)
- `postRequest(RequestContext)`: Execute a single request
- `postBatchRequest(BatchRequestPtrTasks)`: Execute batch requests
- `stopRequest(quint64)`: Stop a specific request
- `stopBatchRequests(quint64)`: Stop batch requests
- `stopAllRequest()`: Stop all active requests
- `setGlobalProxy(ProxyConfig)`: Set global proxy (applied to all requests)
- `setCookieStoragePath(path)`: Enable persistent cookie jar with file path
- `setMaxThreadCount(int)`: Set thread pool size (1-100)

**Signals:**

- `downloadProgress(quint64, qint64, qint64)`: Download progress for a single request.
- `uploadProgress(quint64, qint64, qint64)`: Upload progress for a single request.
- `batchDownloadProgress(quint64, qint64)`: Aggregated download progress for a batch of requests.
- `batchUploadProgress(quint64, qint64)`: Aggregated upload progress for a batch of requests.

#### RequestContext

Configuration structure for network requests (replaces the old RequestTask).

**Key Properties:**

- `url`: Target URL
- `type`: Request type (Download, Upload, Get, Post, Put, Delete, Head)
- `headers`: Request headers (QMap<QByteArray, QByteArray>)
- `body`: Request body for POST/PUT
- `behavior.showProgress`: Enable progress reporting
- `behavior.retryOnFailed`: Enable retry on transient failures
- `behavior.maxRetryCount`: Maximum retry attempts (default: 3)
- `behavior.retryDelayMs`: Base retry delay in ms (doubles each attempt, default: 1000)
- `behavior.maxRedirectionCount`: Maximum redirect limit (default: 3)
- `behavior.priority`: Request priority (higher = more urgent, default: 0)
- `behavior.transferTimeout`: Transfer timeout in ms (default: 30000)
- `proxyConfig`: Per-request proxy override (overrides global proxy)
- `downloadConfig`: Download configuration (saveDir, overwriteFile, threadCount)
- `uploadConfig`: Upload configuration (filePath, usePutMethod, useFormData, useStream)
- `userContext`: User-defined context data

#### NetworkReply

Handles the asynchronous response for a single request or a batch of requests.

**Signals:**

- `requestFinished(QSharedPointer<ResponseResult>)`: Emitted when the request is complete (either successfully or with an error).

#### ResponseResult

Structure containing request response data.

**Key Properties:**

- `success`: Whether the request succeeded
- `cancelled`: Whether the request was cancelled
- `errorMessage`: Error message if failed
- `body`: Response body data
- `headers`: Response headers
- `performance.durationMs`: Request duration in milliseconds
- `performance.bytesReceived`: Bytes received
- `performance.bytesSent`: Bytes sent
- `userContext`: User-defined context data

#### DownloadConfig

Configuration structure for download operations.

**Key Properties:**

- `saveFileName`: Custom filename for the downloaded file
- `saveDir`: Directory to save the downloaded file
- `overwriteFile`: Whether to overwrite existing files (default: false)
- `threadCount`: Number of download threads for multi-threaded downloads (default: 0 = auto detect CPU cores)

#### UploadConfig

Configuration structure for upload operations.

**Key Properties:**

- `filePath`: Path to the file to upload
- `data`: Raw data to upload
- `usePutMethod`: Use HTTP PUT method instead of POST (default: false)
- `useStream`: Use streaming upload (default: false)
- `useFormData`: Use multipart form data (default: false)

## Building

### Build Configuration

The library supports both CMake and QMake build systems with standardized naming:

- **CMake**: Modern, cross-platform build system with automatic dependency detection
- **QMake**: Traditional Qt build system with Visual Studio integration

### Build Targets

**CMake Targets:**

- `QNetworkRequest`: Core library (DLL)
- `QtRequester`: GUI demo application (source in `samples/networkrequesttool/`)
- `QtDownloader`: Download manager application (source in `samples/networkdownloader/`)
- `UnitTests`: Test suite

**QMake Targets:**

- `QNetworkRequest`: Core library (DLL)
- `QtRequester`: GUI demo application (source in `samples/networkrequesttool/`)
- `QtDownloader`: Download manager application (source in `samples/networkdownloader/`)

### Build Types

```bash
# CMake Release build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# CMake Debug build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# QMake build
qmake -r -tp vc QtNetworkRequest.pro
nmake release
nmake debug
```

### Build Scripts

```bash
# Windows build script
scripts\build_win.bat

# Generate Visual Studio solution
GenerateVsSln.bat
```

### Cross-Platform Building

#### Using CMake Directly

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

#### Using Build Scripts

```bash
# Windows (batch script)
scripts\build_win.bat

# Linux (shell script)
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh --release
```

## Cross-Platform Compatibility

QtMultiThreadNetwork is designed to work across multiple platforms with proper build configuration.

### Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| **Windows** | ✅ Fully Supported | Primary development platform, MSVC 2017+ |
| **Linux** | ✅ Fully Supported | GCC 7+ or Clang 6+, CMake 3.15+ |
| **macOS** | ✅ Fully Supported | Xcode 10+ (Clang), CMake 3.15+ |

### Component Compatibility

| Component | Windows | Linux | macOS | Status |
|-----------|---------|-------|-------|--------|
| Core Network Functions | ✅ | ✅ | ✅ | Based on Qt Network (cross-platform) |
| Memory-Mapped Files | ✅ | ✅ | ✅ | Implemented with platform-specific APIs |
| Multi-threaded Downloads | ✅ | ✅ | ✅ | Compatible across platforms |
| Thread Pool Management | ✅ | ✅ | ✅ | Uses Qt's threading framework |
| Build System | ✅ | ✅ | ✅ | CMake with platform-specific optimizations |
| OpenSSL Integration | DLL | Dynamic | Dynamic | Platform-specific linking |
| GUI Applications | ✅ | ✅ | ✅ | Conditional WIN32 flag for proper GUI apps |

### Platform-Specific Implementations

#### Memory-Mapped Files

- **Windows**: Uses `CreateFileMapping` / `MapViewOfFile`
- **Linux/macOS**: Uses `mmap` / `munmap`
- **File Size**: Supports large files (>4GB) on all platforms

#### Build Requirements

#### Windows

- **Compiler**: MSVC 2017+
- **Qt**: 5.6.x+
- **OpenSSL**: DLL files (libeay32.dll, ssleay32.dll)

#### Linux

- **Compiler**: GCC 7+ or Clang 6+
- **Qt**: 5.6.x+
- **OpenSSL**: Development packages (libssl-dev, libcrypto-dev)
- **Build**: CMake 3.15+

#### macOS

- **Compiler**: Xcode 10+ (Clang)
- **Qt**: 5.6.x+
- **OpenSSL**: Homebrew (`brew install openssl@1.1`)
- **Build**: CMake 3.15+
- **Architecture**: Supports both Intel (`x86_64`) and Apple Silicon (`arm64`). Use `-DCMAKE_OSX_ARCHITECTURES` to target a specific architecture if it differs from your Qt binaries.

### Platform-Specific Notes

1. **OpenSSL Handling**: Platform-specific dynamic library linking (Windows DLLs, Unix dynamic libraries)
2. **Build System**: CMake automatically detects and configures platform-specific settings

### Cross-Platform Build Instructions

```bash
# Linux/macOS build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Find and link OpenSSL automatically on Unix systems
find_package(OpenSSL REQUIRED)
target_link_libraries(QNetworkRequest PRIVATE OpenSSL::SSL OpenSSL::Crypto)
```

### Migration Guide

The build system now supports cross-platform compilation with automatic platform detection:

1. **Automatic GUI Flag**: The WIN32 flag is automatically applied only on Windows for GUI executables
2. **OpenSSL Detection**: CMake automatically finds and links OpenSSL on all platforms
3. **Cross-Platform Paths**: CMake handles platform-specific path separators
4. **Threading**: Thread pool management works identically across all platforms

The core library functionality is platform-agnostic and works seamlessly across all supported platforms with proper build configuration.

## Testing

### Running Tests

```bash
# Using CTest
cd build
ctest -C Release

# Direct execution
./test/Release/UnitTests.exe
```

### Test Coverage

The test suite covers:

- Basic request functionality (GET, POST, PUT, DELETE, HEAD, form data)
- File download (single-threaded and multi-threaded)
- File upload (PUT with file)
- Proxy configuration (global and per-request)
- Retry behavior (retry on transient failures, disabled retries)
- Persistent cookie jar (save, reload, cross-request cookie sharing)
- Request priority queue (thread pool saturation with priority ordering)
- Request cancellation (stop running, stop batch, stop all)
- Rapid cancel stress (20x create/cancel cycle)
- Error handling scenarios
- Progress reporting
- Thread safety
- Memory management
- Platform-specific implementations

## Contributing

We welcome contributions! Please follow these guidelines:

1. **Fork the repository** and create a feature branch
2. **Follow the existing code style** and patterns
3. **Add tests** for new functionality
4. **Update documentation** as needed
5. **Submit a pull request** with a clear description

### Development Setup

```bash
# Clone with submodules
git clone --recursive https://github.com/lucaswang420/qt-network-request.git
cd qt-network-request

# Setup development build
cmake -S . -B build-dev -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-dev --config Debug
```

### Code Style

- Use C++17 features and modern C++ practices
- Follow Qt coding conventions
- Use smart pointers for memory management
- Implement proper error handling
- Add comprehensive documentation

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright (c) 2025-2026 Lucas Wang. Licensed under the MIT License.**
