/*
@Brief:			Qt multi-threaded network request module

The Qt multi-threaded network request module is a wrapper of Qt Network module, and combine with thread-pool to realize multi-threaded networking.
- Multi-task concurrent(Each request task is executed in different threads).
- Both single request and batch request mode are supported.
- Large file multi-thread downloading supported. (The thread here refers to the download channel. Download speed is faster.)
- HTTP(S)/FTP protocol supported.
- Multiple request methods supported. (GET/POST/PUT/DELETE/HEAD)
- Asynchronous API.
- Thread-safe.

Note: You must call NetworkRequestManager::initialize() before use, and call NetworkRequestManager::unInitialize() before application quit.
That must be called in the main thread.

MIT License

Copyright (c) 2025 Lucas Wang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <memory>
#include <QMap>
#include <QByteArray>
#include <QVariant>
#include <QNetworkCookie>
#include <QDateTime>
#include <QSharedPointer>
#include <QNetworkProxy>
#ifndef QT_NO_SSL
#include <QSslCertificate>
#include <QSslError>
#endif

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
    // Supported protocols: HTTP(S)/FTP
    // Supported HTTP(S) request methods: GET/POST/PUT/DELETE/HEAD

    enum class RequestType : int32_t
    {
        // Download (supports HTTP(S) and FTP)
        Download = 0x000,
        // Multi-Thread Download (supports HTTP(S))
        MTDownload = 0x001,
        // Upload (supports HTTP(S) and FTP)
        Upload = 0x002,
        // GET method (supports HTTP(S) and FTP)
        Get = 0x003,
        // POST method (supports HTTP(S))
        Post = 0x004,
        // PUT method (supports HTTP(S) and FTP)
        Put = 0x005,
        // DELETE method (supports HTTP(S))
        Delete = 0x006,
        // HEAD method (supports HTTP(S))
        Head = 0x007,
        // PATCH method (supports HTTP(S))
        Patch = 0x008,
        // OPTIONS method (supports HTTP(S))
        Options = 0x009,

        Unknown = -1,
    };

    // 任务元数据
    struct TaskData
    {
        quint64 id{ 0 };
        quint64 batchId{ 0 };
        quint64 sessionId{ 0 };
        bool abortBatchOnFailed{ false };
        QDateTime createTime;
        QDateTime startTime;
        QDateTime endTime;
    };

    struct DownloadConfig;
    struct UploadConfig;
    struct ProxyConfig;

    // 代理配置
    struct ProxyConfig
    {
        bool enabled{ false };
        QNetworkProxy::ProxyType type{ QNetworkProxy::HttpProxy };
        QString host;
        quint16 port{ 1080 };
        QString user;
        QString password;

        QNetworkProxy toQNetworkProxy() const
        {
            if (user.isEmpty())
                return QNetworkProxy(type, host, port);
            return QNetworkProxy(type, host, port, user, password);
        }
    };

#ifndef QT_NO_SSL
    // SSL/TLS 安全策略配置
    // 两级配置：全局默认 (NetworkRequestManager::setGlobalSslConfig) + 每请求覆盖 (RequestContext::sslConfig)。
    // 每请求字段为 Inherit 时继承全局对应值；全局字段经 setGlobalSslConfig 规范化后永不为 Inherit。
    struct SslConfig
    {
        // 证书对端验证模式
        enum class PeerVerifyMode : int8_t
        {
            Inherit    = -1,  // per-request: 继承全局默认
            VerifyNone = 0,   // 不验证（不安全，仅内网/测试）
            VerifyPeer = 1,   // 验证对端证书（安全默认）
        };
        // TLS 最低协议版本
        enum class TlsProtocol : int8_t
        {
            Inherit     = -1,
            TlsV1_0     = 0,
            TlsV1_1     = 1,
            TlsV1_2     = 2,  // 安全默认
            TlsV1_3     = 3,  // Qt 5.12+
            AnyProtocol = 4,  // 由 Qt/系统决定
        };
        // SSL 错误忽略策略
        enum class IgnorePolicy : int8_t
        {
            Inherit              = -1,  // per-request: 继承全局
            Never                = 0,   // 不忽略任何错误（安全默认）
            IgnoreSpecificErrors = 1,   // 仅忽略 ignoreErrorTypes 中列出的错误类型
            Always               = 2,   // 忽略所有 sslErrors（NOT FOR PRODUCTION，需审计日志）
        };
        // CA 证书策略
        enum class CaPolicy : int8_t
        {
            Inherit       = -1,  // per-request: 继承全局
            SystemDefault = 0,   // 使用系统默认 CA
            Custom        = 1,   // 使用 caCertificates 字段（替换系统 CA）
        };

        PeerVerifyMode peerVerifyMode{ PeerVerifyMode::Inherit };
        TlsProtocol    minProtocol{ TlsProtocol::Inherit };
        IgnorePolicy   ignoreSslErrorsPolicy{ IgnorePolicy::Inherit };
        CaPolicy       caPolicy{ CaPolicy::Inherit };
        QList<QSslCertificate> caCertificates;        // 仅 CaPolicy::Custom 时使用
        QList<QSslError::SslError> ignoreErrorTypes;  // 仅 IgnorePolicy::IgnoreSpecificErrors 时使用

        // 安全默认配置（用于全局默认初始化）
        static SslConfig secureDefault()
        {
            SslConfig c;
            c.peerVerifyMode        = PeerVerifyMode::VerifyPeer;
            c.minProtocol           = TlsProtocol::TlsV1_2;
            c.ignoreSslErrorsPolicy = IgnorePolicy::Never;
            c.caPolicy              = CaPolicy::SystemDefault;
            return c;
        }
    };
#endif

    // 请求上下文 (Input)
    struct RequestContext
    {
        // Request type: Upload/Download/Other requests
        RequestType type{ RequestType::Unknown };
        // url
       // Note: For FTP upload, the URL must specify the filename. e.g., "ftp://10.0.192.47:21/upload/test.zip", the file will be saved as test.zip.
        QString url;
        // Request header information
        QMap<QByteArray, QByteArray> headers;
        // Request body
        // case Post:   POST parameters. e.g., "a=b&c=d". or json data
        QString body;
        QList<QNetworkCookie> cookies;

        TaskData task;

        // 行为配置
        struct Behavior
        {
            bool showProgress{ false };
            bool retryOnFailed{ false };
            quint16 maxRetryCount{ 3 };
            int retryDelayMs{ 1000 };
            quint16 maxRedirectionCount{ 3 };
            int transferTimeout{ 30000 }; // 30 seconds (transfer timeout)
            int idleTimeoutMs{ 0 };       // Idle/stall timeout (ms), 0=disabled. If no data received for this duration, abort.
            int totalTimeoutMs{ 0 };      // Total request timeout (ms), 0=disabled. The entire request lifecycle must not exceed this.
            int priority{ 0 }; // higher = more urgent, default 0
        } behavior;

        std::unique_ptr<ProxyConfig> proxyConfig;
#ifndef QT_NO_SSL
        std::unique_ptr<SslConfig> sslConfig;
#endif
        std::unique_ptr<DownloadConfig> downloadConfig;
        std::unique_ptr<UploadConfig> uploadConfig;

        // 用户自定义上下文
        QVariant userContext;
    };
    typedef std::vector<std::unique_ptr<RequestContext>> BatchRequestPtrTasks;

    // 响应结果 (Output)
    struct ResponseResult
    {
        bool success{ false };
        bool cancelled{ false };
        bool timeout{ false };  // true if request terminated due to timeout (not cancellation)
        int statusCode{ 0 };
        int errorCode{ 0 };     // QNetworkReply::NetworkError when failed
        QString errorMessage;
        QByteArray body;
        QMap<QByteArray, QByteArray> headers;

        TaskData task;

        // 用户自定义上下文
        QVariant userContext;

        // 性能统计
        struct Performance
        {
            quint64 durationMs{ 0 };
            qint64 bytesReceived{ 0 };
            qint64 bytesSent{ 0 };
        } performance;
    };

    // 下载配置
    struct DownloadConfig
    {
        QString saveFileName;
        QString saveDir;
        bool overwriteFile{ false };
        quint16 threadCount{ 0 }; // 0 = auto detect CPU cores
    };

    // 上传配置
    struct UploadConfig
    {
        // 非formdata方式
        QString filePath;
        QByteArray data;
        bool usePutMethod{ false };
        bool useStream{ false };
        
        // formdata方式
        bool useFormData{ false };
        QStringList files;
        QMap<QString, QString> kvPairs;
    };
}
Q_DECLARE_METATYPE(QSharedPointer<QtNetworkRequest::ResponseResult>);

#pragma pack(pop)
