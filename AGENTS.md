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
- Qt version compatibility centralized in `source/qtcompat.h` — all `#if QT_VERSION` blocks consolidated into inline adapters (transferTimeout, error signals, Http2Allowed, TLS 1.3, QRecursiveMutex, etc.)
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
- `QtCompat` — centralized Qt version adapters (`source/qtcompat.h`); all `#if QT_VERSION` blocks consolidated
- `ProgressThrottle` — reusable progress rate-limiter (`source/progressthrottle.h/.cpp`); shared by download/upload/MT-download
- `NetworkRequestRegistry` — self-registering factory (`source/networkrequestregistry.h/.cpp`); replaces hard-coded switch-case, uses priority-based conditional routing
- `IMDTDownloadState` / `ProbeState` / `RangeProbeState` / `MultiDownloadState` — state machine for `NetworkMTDownloadRequest` multi-phase lifecycle (`source/networkmtdownloadrequest_p.h`, `source/networkmtdownloadrequest_states.cpp`)
- `requeststrategies.h/.cpp` — strategy interfaces + default implementations for pluggable request behavior (retry, redirect, SSL)

## Directory map

- `cmake/` — CMake modules (compiler flags, OpenSSL detection, utilities)
- `include/` — public headers (10 files: requestcontext.h, networkerror.h, responseresult.h, sslconfig.h, proxyconfig.h, taskdata.h, networkrequestmanager.h, networkreply.h, networkrequestevent.h, networkrequestglobal.h)
- `source/` — implementation + private headers (25+ files)
  - Core: `networkrequest.h/.cpp`, `networkcommonrequest.*`, `networkdownloadrequest.*`, `networkuploadrequest.*`, `networkmtdownloadrequest.*`
  - State machine: `networkmtdownloadrequest_p.h` (state interface), `networkmtdownloadrequest_states.cpp` (ProbeState, RangeProbeState, MultiDownloadState)
  - Strategies: `requeststrategies.h/.cpp` (IRetryStrategy, IRedirectHandler, ISslPolicy)
  - Cross-cutting: `qtcompat.h` (Qt version adapters), `progressthrottle.h/.cpp` (progress rate-limiter), `networkrequestregistry.h/.cpp` (self-registering factory)
  - Infrastructure: `networkrequestmanager.cpp`, `networkrequestrunnable.*`, `networkaccessmanagerpool.*`, `memorymappedfile.*`, `networkrequestutility.*`, `networkreply.cpp`, `networkcookiejar.*`, `sharedcookiejar.h`
- `test/` — Qt Test unit tests (5 suites: UnitTests, BuilderTests, UtilityTests, DownloaderTests, UiTests)
- `samples/networkrequesttool/` — GUI demo
- `samples/networkdownloader/` — download manager app
- `ThirdParty/openssl/` — bundled OpenSSL for Windows
- `scripts/` — platform build scripts
