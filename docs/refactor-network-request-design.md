# 技术方案设计：Network*Request 类族重构

| 属性 | 内容 |
|:---|:---|
| **文档版本** | v1.0 |
| **创建日期** | 2026-07-27 |
| **作者** | CodeBuddy AI |
| **状态** | Draft |
| **关联模块** | `source/network*request.*` 全部请求处理类 |

---

## 1. 背景与动机

### 1.1 现状

`QtMultiThreadNetwork` 项目当前有 5 个网络请求处理类，均继承自 `NetworkRequest` 抽象基类：

| 类 | 职责 |
|:---|:---|
| `NetworkCommonRequest` | GET / POST / PUT / DELETE / HEAD / PATCH / OPTIONS |
| `NetworkDownloadRequest` | 单线程流式文件下载 |
| `NetworkUploadRequest` | 文件 / 表单上传 |
| `NetworkMTDownloadRequest` | 多线程分片下载（HEAD → Range探测 → 分片） |
| `Downloader`（内部类） | 单分片下载器 |

### 1.2 重构动因

经过全量代码审查，发现以下维度存在显著改进空间：

1. **代码重复率过高**：`start()` 和 `onFinished()` 约 60%-70% 逻辑在 4 个子类中重复（约 450 行重复代码）
2. **重定向 / 重试 / SSL 错误处理散落多处**：同一逻辑在 5 个位置独立实现，行为一致性靠人工保证
3. **工厂方法违反开闭原则**：`NetworkRequestFactory::create()` 使用硬编码 `switch-case`，新增类型必须修改工厂
4. **`MTDownloadRequest` 多阶段状态隐式编码**：`m_pNetworkReply` 在三个阶段语义不同，状态转换不可见
5. **Qt 版本兼容代码散落**：`#if QT_VERSION` 在约 15 处出现，新增 Qt6 支持时修改成本高

---

## 2. 现状架构分析

### 2.1 当前类图

```
                         QObject
                            ▲
                            │
               ┌────────────┴────────────┐
               │    NetworkRequest        │
               │  (抽象基类，约 300 行)      │
               │  - start()               │
               │  - onFinished()          │
               │  - tryRetry()            │
               │  - onSslErrors()         │
               │  - onHeartbeat()         │
               └──────┬──────┬──────┬─────┘
                      │      │      │
         ┌────────────┤      │      └──────────────────┐
         │            │      │                         │
  NetworkCommonReq   ...   NetworkMTDownloadReq        │
  (GET/POST/PUT/...)       (分片下载)                   │
                         ┌──────────────────────────┐  │
                         │ 内部类: Downloader        │  │
                         │ (重定向/SSL/进度 全部重复)  │  │
                         └──────────────────────────┘  │
                                          ...          │
```

### 2.2 重复代码热力图

```
                    start()  onFinished() 重定向 SSL错误 进度节流 Qt兼容
NetworkCommonReq      ██         ██         ██     ██      -     ██
NetworkDownloadReq    ██         ██         ██     ██     ██     ██
NetworkUploadReq      ██         ██         ██     ██     ██     ██
NetworkMTDownloadReq  ██(特殊)   ██(3阶段)   ██     ██      -     ██
Downloader(内部类)     -         ██         ██     ██     ██     ██
                      
██ = 存在重复, 与基类或其他子类高度相似
```

### 2.3 量化指标

| 指标 | 当前值 |
|:---|:---|
| 请求处理类总数 | 5（含内部类 6） |
| `start()` 平均重复行数/子类 | ~35 行 |
| `onFinished()` 平均重复行数/子类 | ~40 行 |
| 重定向逻辑出现位置 | 5 处 |
| Qt 版本 `#if` 块 | 15+ 处 |
| 工厂分派分支数 | 8 |
| 公共头暴露成员数量（基类） | 20+ |

---

## 3. 重构目标

| 目标 | 度量标准 |
|:---|:---|
| 消除重复代码 | `start()` / `onFinished()` 重复度降至 0 |
| 行为单一化 | 策略逻辑（重试/重定向/SSL）各仅一处实现 |
| 开闭工厂 | 新增 RequestType 不修改工厂代码 |
| 状态显式化 | `MTDownloadRequest` 阶段转换可追溯 |
| 版本兼容集中化 | Qt 版本差异仅一处维护 |
| 可测试性 | 策略对象可独立单测，钩子可 Mock |
| 行为零退化 | 所有现有测试通过 |

---

## 4. 目标架构设计

### 4.1 整体架构

```
┌──────────────────────────────────────────────────────────────────┐
│                    NetworkRequest （Template Method）             │
│                                                                   │
│  ┌─────────────┐ ┌──────────────┐ ┌──────────────┐               │
│  │ IRetryStrat │ │IRedirectHndlr│ │  ISslPolicy  │  策略层       │
│  └─────────────┘ └──────────────┘ └──────────────┘               │
│  ┌─────────────┐ ┌──────────────┐ ┌──────────────┐               │
│  │IProgressThr │ │ITimeoutStrat │ │   QtCompat   │  工具层       │
│  └─────────────┘ └──────────────┘ └──────────────┘               │
│                                                                   │
│  final start()           ◄── 不可覆写骨架                          │
│  final onFinished()      ◄── 子类不可篡改流程                      │
│                                                                   │
│  // 钩子方法 — 子类唯一需要关心的部分                                │
│  virtual onPreStart()             // 阶段0: 初始化准备              │
│  virtual buildRequest()           // 阶段1: 构建请求                │
│  virtual executeRequest()         // 阶段2: 发送请求                │
│  virtual connectExtraSignals()    // 阶段3: 连接额外信号            │
│  virtual processResponseBody()    // 阶段4: 处理响应体              │
│  virtual onPostFinish()           // 阶段5: 善后清理                │
└──────────────────────────────────────────────────────────────────┘
                              ▲
        ┌─────────────────────┼──────────────────────────┐
        │                     │                          │
  CommonRequest          DownloadRequest          UploadRequest
  (仅覆写钩子方法)         (流式写文件)               (文件上传)
        
   ┌──────────────────────┐
   │  MTDownloadRequest   │
   │  ┌────────────────┐  │
   │  │ StateMachine   │  │   Probe → RangeProbe → MultiDownload → Complete
   │  │  + transition()│  │   每个 State 是独立类，拥有自己的 onFinished()
   │  └────────────────┘  │
   │  ┌────────────────┐  │
   │  │ Downloader[]   │  │   复用 IProgressThrottle + ISslPolicy
   │  └────────────────┘  │
   └──────────────────────┘

NetworkRequestRegistry  ◄── 自注册工厂, 条件路由, 扩展开放
```

### 4.2 设计模式选型与职责

| 设计模式 | 应用位置 | 解决什么问题 |
|:---|:---|:---|
| **Template Method** | `NetworkRequest` 基类 | `start()` / `onFinished()` 骨架统一，子类只填钩子 |
| **Strategy** | 重试/重定向/SSL/超时/进度 | 行为参数化，运行时可替换，独立可测 |
| **State** | `NetworkMTDownloadRequest` | 三阶段生命周期显式化，每个状态独立类 |
| **Registry + Factory** | `NetworkRequestRegistry` | 新增类型自注册，不修改工厂，支持条件路由 |
| **Decorator（组合式）** | `ProgressThrottle` | 进度节流作为独立组件注入，3 处→1 处 |
| **Adapter** | `QtCompat` 命名空间 | 集中管理 Qt 版本差异，单点维护 |
| **Builder** | `RequestContextBuilder`（已有，增强） | 支持注入策略配置 |

---

## 5. 详细设计

### 5.1 模板方法骨架 — `NetworkRequest`

```cpp
// include/networkrequest.h（重构后）

class NETWORK_EXPORT NetworkRequest : public QObject
{
    Q_OBJECT

public:
    // ===== 模板方法（final，不可覆写）=====
    void start() final;
    void abort() final;

    // ===== 构建器注入策略对象 =====
    void setRetryStrategy(std::unique_ptr<IRetryStrategy> s);
    void setRedirectHandler(std::unique_ptr<IRedirectHandler> h);
    void setSslPolicy(std::unique_ptr<ISslPolicy> p);
    void setProgressThrottle(std::unique_ptr<IProgressThrottle> t);

Q_SIGNALS:
    void response(QSharedPointer<ResponseResult> result);

protected:
    // ===== 钩子方法（子类选择性覆写）=====
    virtual bool   onPreStart();                              // 默认 true
    virtual QNetworkRequest buildRequest() = 0;               // 纯虚
    virtual QNetworkReply* executeRequest(QNetworkRequest& r) = 0; // 纯虚
    virtual void   connectExtraSignals(QNetworkReply* reply);  // 默认空
    virtual bool   processResponseBody(QNetworkReply* reply);  // 默认 true
    virtual void   onPostFinish();                             // 默认空

    // ===== 内部骨架步骤 =====
    bool   validateAndPrepare();
    void   applyCommonConfig(QNetworkAccessManager* nam);
    void   connectCommonSignals(QNetworkReply* reply);
    bool   handleError(QNetworkReply* reply);
    bool   handleRedirectIfNeeded(QNetworkReply* reply);
    void   cleanupReply(QNetworkReply* reply);
    void   populateResponseHeaders(QNetworkReply* reply);

    // 持有
    RequestContextPtr                m_upContext;
    QSharedPointer<ResponseResult>   m_spResult;
    QNetworkAccessManager*           m_pNetworkManager = nullptr;
    QNetworkReply*                   m_pNetworkReply   = nullptr;

    std::unique_ptr<IRetryStrategy>   m_retryStrategy;
    std::unique_ptr<IRedirectHandler> m_redirectHandler;
    std::unique_ptr<ISslPolicy>       m_sslPolicy;
    std::unique_ptr<IProgressThrottle>m_progressThrottle;
};
```

```cpp
// source/networkrequest.cpp — 骨架实现

void NetworkRequest::start()
{
    // 步骤 1: 校验
    if (!validateAndPrepare()) {
        emit response(ToFailedResult());
        return;
    }

    // 步骤 2: 子类前置钩子
    if (!onPreStart()) {
        emit response(ToFailedResult());
        return;
    }

    // 步骤 3: 获取 NAM + 应用公共配置（代理/Cookie/SSL/头/超时）
    if (nullptr == m_pNetworkManager)
        m_pNetworkManager = NetworkRequestManager::acquireThreadNam();
    applyCommonConfig(m_pNetworkManager);

    // 步骤 4: 子类构建请求
    QNetworkRequest req = buildRequest();

    // 步骤 5: 子类发送请求
    m_pNetworkReply = executeRequest(req);

    // 步骤 6: 统一连接核心信号
    connectCommonSignals(m_pNetworkReply);

    // 步骤 7: 子类连接额外信号
    connectExtraSignals(m_pNetworkReply);

    // 步骤 8: 启动心跳（idle/transfer timeout）
    startHeartbeatMonitor();
}

void NetworkRequest::onFinished()
{
    // 该槽连接到 m_pNetworkReply->finished，子类不再直接处理
    if (!m_pNetworkReply)
        return;

    // 步骤 1: 错误处理（包含重试 + 重定向）
    if (handleError(m_pNetworkReply))
        return;

    // 步骤 2: 子类处理响应体
    if (!processResponseBody(m_pNetworkReply))
        return;

    // 步骤 3: 填充响应头
    populateResponseHeaders(m_pNetworkReply);

    // 步骤 4: 清理
    cleanupReply(m_pNetworkReply);

    // 步骤 5: 子类后置钩子
    onPostFinish();

    emit response(ToSuccessResult());
}
```

**子类示例 — `NetworkCommonRequest` 简化后：**

```cpp
class NetworkCommonRequest : public NetworkRequest
{
protected:
    QNetworkRequest buildRequest() override {
        QNetworkRequest req(m_url);
        setCommonHeaders(req);
        setAuthHeaders(req);
        return req;
    }

    QNetworkReply* executeRequest(QNetworkRequest& req) override {
        switch (m_upContext->type) {
            case RequestType::Get:    return m_pNetworkManager->get(req);
            case RequestType::Post:   return m_pNetworkManager->post(req, buildBody());
            case RequestType::Put:    return m_pNetworkManager->put(req, buildBody());
            case RequestType::Delete: return m_pNetworkManager->deleteResource(req);
            case RequestType::Head:   return m_pNetworkManager->head(req);
            default:                  return m_pNetworkManager->sendCustomRequest(req, verb());
        }
    }

    bool processResponseBody(QNetworkReply* reply) override {
        m_spResult->responseData  = reply->readAll();
        m_spResult->responseBody  = m_spResult->responseData;
        return true;
    }
    // 不覆写 onPreStart / connectExtraSignals / onPostFinish — 使用默认空实现
};
```

对比重构前约 120 行 → 重构后约 30 行。

---

### 5.2 策略接口设计

```cpp
// source/requeststrategies.h

// ===== 重试策略 =====
struct IRetryStrategy {
    virtual ~IRetryStrategy() = default;
    virtual bool shouldRetry(ErrorCode code, int attemptCount, int maxRetries) const = 0;
    virtual int  delayMs(int attemptCount) const = 0;
};

class ExponentialBackoffRetry : public IRetryStrategy {
    int m_maxRetries{3};
    int m_baseDelayMs{1000};
    int m_maxDelayMs{30000};
public:
    bool shouldRetry(ErrorCode code, int n, int max) const override {
        return n < max && isTransientError(code);
    }
    int delayMs(int n) const override {
        return std::min(m_baseDelayMs * (1 << (n - 1)), m_maxDelayMs);
    }
};

// ===== 重定向处理器 =====
struct IRedirectHandler {
    virtual ~IRedirectHandler() = default;
    virtual bool shouldRedirect(int statusCode, int redirectCount, int maxRedirects) const = 0;
    virtual QUrl resolveRedirect(const QUrl& current, const QUrl& target) const = 0;
};

class StandardRedirectHandler : public IRedirectHandler {
    int m_maxRedirects{10};
public:
    bool shouldRedirect(int sc, int count, int max) const override {
        return (sc == 301 || sc == 302) && count < max;
    }
    QUrl resolveRedirect(const QUrl&, const QUrl& target) const override {
        return target; // 默认直接使用目标
    }
};

// ===== SSL 策略 =====
enum class SslAction { Accept, Reject, Ignore };

struct ISslPolicy {
    virtual ~ISslPolicy() = default;
    virtual SslAction onSslErrors(const QList<QSslError>& errors,
                                   const SslConfig& config,
                                   SslIgnoreErrors resolvedPolicy) const = 0;
};

class DefaultSslPolicy : public ISslPolicy {
public:
    SslAction onSslErrors(const QList<QSslError>& errors,
                           const SslConfig& config,
                           SslIgnoreErrors resolvedPolicy) const override {
        switch (resolvedPolicy) {
            case SslIgnoreErrors::Always:            return SslAction::Ignore;
            case SslIgnoreErrors::IgnoreSpecificErrors:
                return isInIgnoreList(errors, config.expectedSslErrors)
                    ? SslAction::Ignore : SslAction::Reject;
            case SslIgnoreErrors::Never:             return SslAction::Reject;
        }
        return SslAction::Reject;
    }
};

// ===== 超时策略 =====
struct ITimeoutStrategy {
    virtual ~ITimeoutStrategy() = default;
    virtual int  idleTimeoutMs()    const = 0;
    virtual int  transferTimeoutMs()const = 0;
    virtual int  totalTimeoutMs()   const = 0;
    virtual void onHeartbeat(int& idleCounter, int idleThreshold) {}
};
```

---

### 5.3 进度节流组件

```cpp
// source/progressthrottle.h

class ProgressThrottle : public QObject
{
    Q_OBJECT
public:
    explicit ProgressThrottle(int intervalMs = 250, QObject* parent = nullptr);

    // 工作线程调用，内部做节流判断 + 信号发射
    void report(qint64 bytesSent, qint64 bytesTotal,
                const std::function<void(qint64, qint64)>& emitter);

    void start();
    void stop();

Q_SIGNALS:
    void throttled(qint64 bytesSent, qint64 bytesTotal);

private:
    QTimer  m_timer;
    bool    m_ready = false;
    qint64  m_sent  = 0;
    qint64  m_total = 0;
    std::function<void(qint64,qint64)> m_emitter;
};
```

使用方式：

```cpp
// 构造时注入
m_progressThrottle = std::make_unique<ProgressThrottle>(250, this);
connect(m_progressThrottle.get(), &ProgressThrottle::throttled,
        this, [this](qint64 s, qint64 t) {
    QCoreApplication::postEvent(targetObject,
        new NetworkProgressEvent(m_id, ProgressType::Download, s, t));
});

// connectExtraSignals 中注册
void NetworkDownloadRequest::connectExtraSignals(QNetworkReply* reply) {
    m_progressThrottle->start();
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 r, qint64 t) {
        m_progressThrottle->report(r, t, [this](qint64 r, qint64 t) {
            // 真正发射在节流放开时
        });
    });
}
```

---

### 5.4 状态机 — `NetworkMTDownloadRequest`

```cpp
// source/networkmtdownloadrequest_p.h — 新增私有头

class IMDTDownloadState {
public:
    virtual ~IMDTDownloadState() = default;
    virtual void enter(NetworkMTDownloadRequest* ctx) = 0;
    virtual void onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply) = 0;
    virtual QString name() const = 0;
};

class ProbeState : public IMDTDownloadState { /* HEAD 请求 */ };
class RangeProbeState : public IMDTDownloadState { /* 探测兼容性 */ };
class FallbackState : public IMDTDownloadState { /* 降级单线程 */ };
class MultiDownloadState : public IMDTDownloadState { /* 分片下载 */ };
class CompleteState : public IMDTDownloadState { /* 完成 */ };

// 状态转换图
//
//    start()
//      │
//   [Probe]  ──HEAD──▶  [RangeProbe]  ──206──▶  [MultiDownload]  ──allDone──▶  [Complete]
//      │                      │                       │
//      └──❌──▶ [Fallback] ◀──┘                       └──❌──▶ [Fallback]
//                   │
//                   └──done──▶ [Complete]

class NetworkMTDownloadRequest : public NetworkRequest {
public:
    void transitionTo(std::unique_ptr<IMDTDownloadState> newState);

private:
    std::unique_ptr<IMDTDownloadState> m_state;
    // ... 其他成员不变，但阶段特定数据移入各 State 类
};
```

---

### 5.5 自注册工厂

```cpp
// source/networkrequestregistry.h

using RequestCreator = std::function<NetworkRequest*(RequestContextPtr)>;

class NetworkRequestRegistry {
public:
    static NetworkRequestRegistry& instance();

    // priority 越大优先级越高
    void registerCreator(RequestType type, int priority, RequestCreator creator);

    // 按优先级遍历，返回第一个成功创建的
    NetworkRequest* create(RequestContextPtr ctx);

private:
    // type → [(priority, creator)] 按 priority 降序排序
    QHash<RequestType, QList<QPair<int, RequestCreator>>> m_registry;
};
```

注册方式（各子类 `.cpp` 中静态自注册）：

```cpp
// source/networkdownloadrequest.cpp

namespace {
    static const int _reg_flag = []() -> int {
        NetworkRequestRegistry::instance().registerCreator(
            RequestType::Download, 10,
            [](RequestContextPtr ctx) -> NetworkRequest* {
                if (ctx->downloadConfig.threadCount > 1)
                    return new NetworkMTDownloadRequest(ctx);
                else
                    return new NetworkDownloadRequest(ctx);
            });
        return 0;
    }();
}
```

```cpp
// source/networkcommonrequest.cpp

namespace {
    static const int _reg_flag = []() -> int {
        for (auto type : {RequestType::Get, RequestType::Post, RequestType::Put,
                          RequestType::Delete, RequestType::Head,
                          RequestType::Patch, RequestType::Options}) {
            NetworkRequestRegistry::instance().registerCreator(
                type, 5,
                [](RequestContextPtr ctx) -> NetworkRequest* {
                    return new NetworkCommonRequest(ctx);
                });
        }
        return 0;
    }();
}
```

---

### 5.6 Qt 版本适配层

```cpp
// source/qtcompat.h
#pragma once
#include <QtGlobal>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace QtCompat {

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    inline void setTransferTimeout(QNetworkRequest& req, int ms) {
        req.setTransferTimeout(ms);
    }
    constexpr auto kErrorSignal = &QNetworkReply::errorOccurred;
#else
    inline void setTransferTimeout(QNetworkRequest&, int) { /* no-op */ }
    // error(QNetworkReply::NetworkError) 在 < 5.15 中
    inline auto errorSignal(QNetworkReply* reply) {
        return QObject::connect(reply,
            QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            /* slot */);
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 兼容入口（预留）
#endif

} // namespace QtCompat
```

---

## 6. 实施计划

### 6.1 总体原则

1. **小步快跑**：每个 Phase 独立 PR，独立合入，独立测试
2. **行为零退化**：重构不改功能，所有现有测试必须通过
3. **向后兼容**：公共头文件中的类名、信号名保持不变
4. **逐类迁移**：一次将一个子类迁移到新骨架，降低风险

### 6.2 阶段划分

```
Phase 1      Phase 2      Phase 3      Phase 4      Phase 5
基础设施      组件提取      模板方法      状态机       工厂注册
   │            │            │            │            │
   │  2 天       │  1 天       │  3 天       │  2 天       │  1 天
   ▼            ▼            ▼            ▼            ▼
┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐
│Qt    │    │Progr │    │重构  │    │重构  │    │注册表│
│Compat│    │essThr│    │基类  │    │MTDo- │    │工厂  │
│+策略 │───▶│ottle │───▶│逐子类│───▶│wnload│───▶│替换  │
│接口  │    │      │    │迁移  │    │+状态 │    │开关  │
└──────┘    └──────┘    └──────┘    └──────┘    └──────┘
  累计:2天     累计:3天    累计:6天    累计:8天    累计:9天
```

---

### 6.3 Phase 1 — 基础设施搭建（预计 2 天）

**目标**：在不修改任何现有行为的前提下，引入策略接口和 Qt 兼容层。

| 任务 | 产出文件 | 工时 |
|:---|:---|:---|
| 1.1 创建 `source/qtcompat.h` | `source/qtcompat.h` | 0.5 天 |
| 1.2 创建 `source/requeststrategies.h` 策略接口 | `source/requeststrategies.h` | 0.5 天 |
| 1.3 实现默认策略类（无状态，纯函数） | `source/requeststrategies.h` | 0.5 天 |
| 1.4 用 `QtCompat` 替换现有 `#if` 块（全局搜索替换） | 多个源文件 | 0.5 天 |

**验收标准**：
- 编译通过（Debug + Release）
- 全部现有单元测试通过
- `QtCompat::setTransferTimeout()` 在 Qt 5.15+ 和 <5.15 下行为正确

**风险**：低。仅做机械替换，不改变控制流。

---

### 6.4 Phase 2 — 进度节流组件提取（预计 1 天）

**目标**：提取 `ProgressThrottle` 组件，消除 3 处重复的节流逻辑。

| 任务 | 产出文件 | 工时 |
|:---|:---|:---|
| 2.1 实现 `ProgressThrottle` 类 | `source/progressthrottle.h/.cpp` | 0.3 天 |
| 2.2 `NetworkDownloadRequest` 接入 | `source/networkdownloadrequest.cpp` | 0.2 天 |
| 2.3 `NetworkUploadRequest` 接入 | `source/networkuploadrequest.cpp` | 0.2 天 |
| 2.4 `Downloader`（MT内部类）接入 | `source/networkmtdownloadrequest.cpp` | 0.2 天 |
| 2.5 删除旧的节流代码，回归测试 | 同上 | 0.1 天 |

**验收标准**：
- 下载/上传进度事件频率与重构前一致（250ms 节流）
- 进度百分比计算精度一致

**风险**：低。`ProgressThrottle` 是纯内部实现，外部无暴露 API。

---

### 6.5 Phase 3 — 模板方法骨架 + 策略注入（预计 3 天）

**目标**：重构 `NetworkRequest` 基类为模板方法，逐个迁移子类。

| 任务 | 工时 |
|:---|:---|
| 3.1 增强基类：添加 final `start()` / `onFinished()` 骨架，策略对象持有，钩子方法 | 1.0 天 |
| 3.2 迁移 `NetworkCommonRequest` 到新骨架 | 0.5 天 |
| 3.3 迁移 `NetworkDownloadRequest` 到新骨架 | 0.5 天 |
| 3.4 迁移 `NetworkUploadRequest` 到新骨架 | 0.5 天 |
| 3.5 迁移 `NetworkMTDownloadRequest`（暂保持状态机之前的行为） | 0.3 天 |
| 3.6 全量回归测试 + 修复 | 0.2 天 |

**验收标准**：
- `requestFinished` 信号参数（URL/状态码/body/响应头/耗时）与重构前一致
- 重试行为一致（次数、延迟、可重试错误类型）
- 重定向行为一致（301/302、最大次数）
- SSL 错误处理策略一致
- Cookie 传递行为一致

**风险**：中。基类骨架变更影响所有子类，需逐一验证。

**回滚方案**：
- 基类保留 `protected` 的旧 `start()` / `onFinished()` 实现作为 fallback
- 子类迁移期间同时存在两个版本，编译期通过宏切换：

```cpp
#ifdef USE_NEW_TEMPLATE_METHOD
    void start() final { /* 新骨架 */ }
#else
    void start() override { /* 旧实现 */ }
#endif
```

- `CMakeLists.txt` 中通过 `option(USE_NEW_TEMPLATE_METHOD "..." ON)` 控制

---

### 6.6 Phase 4 — MTDownloadRequest 状态机重构（预计 2 天）

**目标**：引入显式状态机，消除 `m_pNetworkReply` 语义歧义和隐式阶段转换。

| 任务 | 工时 |
|:---|:---|
| 4.1 设计并实现 5 个状态类 | 0.8 天 |
| 4.2 重构 `MTDownloadRequest`：用 `transitionTo()` 替代隐式分阶段判断 | 0.7 天 |
| 4.3 将 `Downloader` 复用策略对象（`ProgressThrottle` / `ISslPolicy`） | 0.3 天 |
| 4.4 回归测试（含大文件分片下载、降级场景） | 0.2 天 |

**验收标准**：
- HEAD 探测 → Range 探测 → 分片下载 → 完成，全流程通过
- 服务器不支持 Range 时正确降级
- CDN 忽略 Range 返回完整文件时溢出检测正常
- 分片连接失败时正确重试
- 临时文件重命名为最终文件名正确

**风险**：中高。`MTDownloadRequest` 逻辑最复杂，建议在 Phase 3 完成且稳定后再进行。

**回滚方案**：保留旧实现文件 `source/networkmtdownloadrequest_legacy.cpp`，编译期通过 CMake option 切换。

---

### 6.7 Phase 5 — 注册表工厂替换（预计 1 天）

**目标**：用自注册工厂替代硬编码 switch-case。

| 任务 | 工时 |
|:---|:---|
| 5.1 实现 `NetworkRequestRegistry` | 0.3 天 |
| 5.2 各子类添加静态自注册代码 | 0.3 天 |
| 5.3 `NetworkRequestFactory::create()` 改为调用 Registry | 0.2 天 |
| 5.4 删除旧的 switch-case | 0.1 天 |
| 5.5 回归测试 + 验证动态加载场景 | 0.1 天 |

**验收标准**：
- 所有 `RequestType` 对应的创建行为与重构前一致
- `threadCount > 1` 时 `Download` 类型路由到 `MTDownloadRequest`
- 编译期静态初始化顺序正确（无崩溃）

**风险**：低。工厂是纯创建层，不影响请求处理逻辑。但需注意静态初始化顺序问题（跨编译单元），通过 `instance()` 的局部静态变量确保。

---

### 6.8 总时间线

```
Week 1                      Week 2
│                            │
├─ Phase 1 (2天) ──────────┤
│  QtCompat + 策略接口       │
│                            ├─ Phase 3 (3天) ──────────┤
├─ Phase 2 (1天) ──────────┤  模板方法 + 子类迁移         │
│  ProgressThrottle          │                            │
│                            │                            ├─ Phase 4 (2天) ─┤
│                            │                            │  MT状态机        │
│                            │                            │                 ├─ Phase 5 (1天) ─┤
│                            │                            │                 │  注册表工厂      │
└────────────────────────────┴────────────────────────────┴─────────────────┴─────────────────┘
                           总工期: ~9 个工作日
```

---

### 6.9 风险矩阵

| 风险 | 概率 | 影响 | 缓解措施 |
|:---|:---:|:---:|:---|
| 模板方法遗漏钩子导致子类行为丢失 | 中 | 高 | 逐类 diff 对比新旧 `start()` 每步骤；增加集成测试 |
| 策略对象生命周期管理错误 | 低 | 中 | 使用 `unique_ptr` 强制所有权；基类析构保证顺序 |
| 静态自注册初始化顺序问题 | 低 | 高 | `Meyer's Singleton` + `Q_GLOBAL_STATIC` 保障 |
| 状态机遗漏降级路径 | 中 | 中 | 状态转换图覆盖率检查；增加降级场景测试用例 |
| Qt 版本兼容适配遗漏 | 低 | 中 | CI 多 Qt 版本矩阵构建 |
| 性能退化 | 低 | 低 | Phase 3 后对比基准测试（请求延迟 P50/P99） |

---

## 7. 测试策略

### 7.1 测试金字塔

```
        ┌──────────────┐
        │   E2E 测试     │  3-5 个端到端场景
        │  (现有测试套件) │  (httpbin + 本地 HTTP Server)
        ├──────────────┤
        │  集成测试      │  10-15 个场景
        │  (完整请求链路) │  (重试/重定向/SSL/超时/降级)
        ├──────────────┤
        │  单元测试 ★    │  30+ 个用例（新增重点）
        │  (策略+组件+钩子)│  每个策略 / 组件 / 状态独立可测
        └──────────────┘
```

### 7.2 各阶段新增测试

| 阶段 | 新增测试内容 |
|:---|:---|
| Phase 1 | `QtCompat` 各版本函数行为；`ExponentialBackoffRetry` 延迟计算边界；`DefaultSslPolicy` 三种策略分支 |
| Phase 2 | `ProgressThrottle` 250ms 节流精度；`start()`/`stop()` 生命周期 |
| Phase 3 | Mock `NetworkRequest` 子类验证骨架步骤调用顺序；各钩子方法默认行为 |
| Phase 4 | 状态转换所有路径；降级触发条件；分片错误重试；溢出检测 |
| Phase 5 | Registry 优先级路由；未注册类型返回 nullptr；静态初始化顺序 |

### 7.3 回归测试保证

- 每个 Phase 合入前必须通过 `test/test_networkrequest.cpp` 全部用例
- Phase 3-5 每个子类迁移后，跑一次针对该类型的专项测试
- CI 流水线配置 Windows (MSVC) + Linux (GCC) + macOS (Clang) 三平台

---

## 8. 文件变更清单

| 文件 | Phase | 变更类型 |
|:---|:---:|:---|
| `source/qtcompat.h` | 1 | **新增** |
| `source/requeststrategies.h` | 1 | **新增** |
| `source/progressthrottle.h` | 2 | **新增** |
| `source/progressthrottle.cpp` | 2 | **新增** |
| `source/networkrequestregistry.h` | 5 | **新增** |
| `source/networkrequestregistry.cpp` | 5 | **新增** |
| `source/networkmtdownloadrequest_p.h` | 4 | **新增**（私有头，状态类） |
| `include/networkrequest.h` | 3 | **修改**（添加 final + 钩子 + 策略持有） |
| `source/networkrequest.h` | 3 | **修改**（私有头同步） |
| `source/networkrequest.cpp` | 1,3 | **修改** |
| `source/networkcommonrequest.h` | 3 | **修改**（精简） |
| `source/networkcommonrequest.cpp` | 3 | **修改**（精简） |
| `source/networkdownloadrequest.h` | 2,3 | **修改** |
| `source/networkdownloadrequest.cpp` | 2,3 | **修改** |
| `source/networkuploadrequest.h` | 2,3 | **修改** |
| `source/networkuploadrequest.cpp` | 2,3 | **修改** |
| `source/networkmtdownloadrequest.h` | 4 | **修改** |
| `source/networkmtdownloadrequest.cpp` | 2,4 | **修改** |
| `source/CMakeLists.txt` | 1,2,4,5 | **修改**（添加新文件） |
| `test/test_networkrequest.cpp` | 全阶段 | **修改**（添加新测试用例） |

---

## 9. 附录：重构前后对比总结

| 维度 | 重构前 | 重构后 |
|:---|:---|:---|
| `start()` 实现方式 | 4 个子类各自实现 ~35 行 | 基类 final 骨架 + 子类覆写 2-3 个钩子 |
| `onFinished()` 实现方式 | 5 处独立实现 ~40 行 | 基类 final 骨架 + 子类覆写 `processResponseBody()` |
| 重试逻辑 | 基类 `tryRetry()` 硬编码 | `IRetryStrategy` 注入，可替换 |
| 重定向逻辑 | 5 处复制粘贴 | `IRedirectHandler` 一处实现 |
| 进度节流 | 3 处手写相同逻辑 | `ProgressThrottle` 组件复用 |
| SSL 错误处理 | 2 处重复 | `ISslPolicy` 一处实现 |
| Qt 版本兼容 | ~15 处 `#if` 散落 | `QtCompat` 命名空间一处维护 |
| 工厂扩展性 | 修改 switch-case | 自注册，不修改核心代码 |
| MTDownloadRequest 状态 | 隐式，`m_pNetworkReply` 多语义 | 显式状态机，每状态独立类 |
| 新增请求类型成本 | ~120 行 + 改工厂 | ~30 行 + 一行注册代码 |
