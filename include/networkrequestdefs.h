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

Copyright (c) 2025 Vilas Wang

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
            bool retryOnFailed{ false };//TODO
            quint16 maxRedirectionCount{ 3 };
            int transferTimeout{ 30000 }; // 30 seconds
        } behavior;

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
            qint64 bytesReceived{ 0 };//TODO
            qint64 bytesSent{ 0 };//TODO
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
