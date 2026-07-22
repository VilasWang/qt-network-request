/*
@Brief:			Qt multi-threaded network request module – request context & builder

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
#include <vector>

#include "taskdata.h"
#include "proxyconfig.h"
#ifndef QT_NO_SSL
#include "sslconfig.h"
#endif

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
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

	// ====================================================================
	// RequestContextBuilder — fluent builder for RequestContext
	// ====================================================================

	class RequestContextBuilder
	{
	public:
		RequestContextBuilder() : m_context(std::make_unique<RequestContext>())
		{
		}

		// --- url ---
		RequestContextBuilder &url(const QString &v)
		{
			m_context->url = v;
			return *this;
		}

		// --- type ---
		RequestContextBuilder &type(RequestType v)
		{
			m_context->type = v;
			return *this;
		}

		// --- headers ---
		RequestContextBuilder &header(const QByteArray &key, const QByteArray &value)
		{
			m_context->headers.insert(key, value);
			return *this;
		}
		RequestContextBuilder &headers(const QMap<QByteArray, QByteArray> &v)
		{
			m_context->headers = v;
			return *this;
		}

		// --- body ---
		RequestContextBuilder &body(const QString &v)
		{
			m_context->body = v;
			return *this;
		}

		// --- cookies ---
		RequestContextBuilder &cookie(const QNetworkCookie &v)
		{
			m_context->cookies.append(v);
			return *this;
		}

		// --- task ---
		RequestContextBuilder &taskId(quint64 v)
		{
			m_context->task.id = v;
			return *this;
		}
		RequestContextBuilder &batchId(quint64 v)
		{
			m_context->task.batchId = v;
			return *this;
		}
		RequestContextBuilder &sessionId(quint64 v)
		{
			m_context->task.sessionId = v;
			return *this;
		}

		// --- behavior ---
		RequestContextBuilder &timeout(int totalMs)
		{
			m_context->behavior.totalTimeoutMs = totalMs;
			return *this;
		}
		RequestContextBuilder &transferTimeout(int ms)
		{
			m_context->behavior.transferTimeout = ms;
			return *this;
		}
		RequestContextBuilder &idleTimeout(int ms)
		{
			m_context->behavior.idleTimeoutMs = ms;
			return *this;
		}
		RequestContextBuilder &retry(int maxCount, int delayMs = 1000)
		{
			m_context->behavior.retryOnFailed = true;
			m_context->behavior.maxRetryCount = static_cast<quint16>(maxCount);
			m_context->behavior.retryDelayMs = delayMs;
			return *this;
		}
		RequestContextBuilder &maxRedirects(int count)
		{
			m_context->behavior.maxRedirectionCount = static_cast<quint16>(count);
			return *this;
		}
		RequestContextBuilder &priority(int v)
		{
			m_context->behavior.priority = v;
			return *this;
		}
		RequestContextBuilder &showProgress(bool v = true)
		{
			m_context->behavior.showProgress = v;
			return *this;
		}

		// --- configs ---
		RequestContextBuilder &proxyConfig(std::unique_ptr<ProxyConfig> v)
		{
			m_context->proxyConfig = std::move(v);
			return *this;
		}
#ifndef QT_NO_SSL
		RequestContextBuilder &sslConfig(const SslConfig &v)
		{
			m_context->sslConfig = std::make_unique<SslConfig>(v);
			return *this;
		}
#endif
		RequestContextBuilder &downloadConfig(std::unique_ptr<DownloadConfig> v)
		{
			m_context->downloadConfig = std::move(v);
			return *this;
		}
		RequestContextBuilder &uploadConfig(std::unique_ptr<UploadConfig> v)
		{
			m_context->uploadConfig = std::move(v);
			return *this;
		}

		// --- user context ---
		RequestContextBuilder &userContext(const QVariant &v)
		{
			m_context->userContext = v;
			return *this;
		}

		// --- build ---
		std::unique_ptr<RequestContext> build()
		{
			return std::move(m_context);
		}

	private:
		std::unique_ptr<RequestContext> m_context;
	};
}

#pragma pack(pop)
