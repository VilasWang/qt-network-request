# AGENTS.md

## Project

**QtMultiThreadNetwork** — C++17 library wrapping Qt5 Network with multi-threaded HTTP(S)/FTP requests. Single `QNetworkRequest` shared library, two sample GUI apps, one test suite.

## Build

Primary: **CMake 3.15+** (QMake alternative also supported).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Debug builds append `d` to the lib name (`QNetworkRequestd`).

**Windows** — set `QT_DIR` or `QTDIR` env var before running `scripts\build_win.bat`. OpenSSL DLLs (`ThirdParty/openssl/`) are auto-copied to build dir per target.

**Linux** — run from `scripts/` dir: `./build_linux.sh --release --tests`

**macOS** — requires `export OPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)` before CMake configure.

## Targets

| Target | Type | Location |
|---|---|---|
| `QNetworkRequest` | Shared library | `source/`, `include/` |
| `QtRequester` | GUI app | `samples/networkrequesttool/` |
| `QtDownloader` | GUI app | `samples/networkdownloader/` |
| `UnitTests` | Test executable | `test/` |

## Tests

Qt Test framework. Test files: `test/test_networkrequest.cpp` (core), `test/test_qtrequester.cpp` (UI), `test/httptestserver.cpp` (local HTTP server for controlled tests).

```powershell
cd build/test
ctest -C Release --output-on-failure
```

Tests use real network requests (httpbin.org) and require SSL support. On Linux, prefix with `xvfb-run --auto-servernum`.

## Key conventions

- **API lifecycle**: `NetworkRequestManager::initialize()` (main thread) → use → `unInitialize()` (main thread)
- **Request construction**: Use `RequestContextBuilder` fluent API (`.url(...).type(...).build()`) — do not manually construct `RequestContext`
- Export macro: `NETWORK_EXPORT` (defined in `include/networkrequestglobal.h`, controlled by `QT_MTNETWORK_LIB` / `QT_MTNETWORK_STATIC`)
- CMake: `CMAKE_AUTOMOC`, `AUTOUIC`, `AUTORCC` are ON
- Qt version switch: QRecursiveMutex used for Qt ≥ 5.14 (`USE_Q_RECURSIVE_MUTEX` define)
- Thread count `0` = auto-detect CPU cores (download config)

## Architecture

Public API in `include/` (9 files); implementation in `source/` (private headers + .cpp). Request pipeline:

`RequestContext` → `NetworkRequestManager::postRequest()` → `NetworkReply` (signals `requestFinished`) → `ResponseResult`

Request types handled by separate classes in `source/`: `NetworkCommonRequest`, `NetworkDownloadRequest`, `NetworkMTDownloadRequest`, `NetworkUploadRequest`, `NetworkRequest`.

Key internal components:
- `MemoryMappedFile` — platform-specific file I/O (CreateFileMapping on Windows, mmap on Unix)
- `NetworkCookieJar` / `SharedCookieJar` — persistent file-backed cookie storage
- `NetworkAccessManagerPool` — QNetworkAccessManager instance pool per thread
- `NetworkRequestRunnable` — QRunnable wrapper for thread-pool execution

## Directory map

- `cmake/` — CMake modules (compiler flags, OpenSSL detection, utilities)
- `include/` — public headers
- `source/` — implementation + private headers
- `test/` — Qt Test unit tests (single suite)
- `samples/networkrequesttool/` — GUI demo
- `samples/networkdownloader/` — download manager app
- `ThirdParty/openssl/` — bundled OpenSSL for Windows
- `scripts/` — platform build scripts
