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

#include <QObject>
#include <atomic>
#include <functional>
#include "requestcontext.h"
#include "responseresult.h"
#include "networkrequestglobal.h"
#include <memory>
#include <QMutex>

class QNetworkAccessManager;
class QNetworkCookieJar;

class QEvent;

namespace QtNetworkRequest
{
	class NetworkRequestManagerPrivate;
	class NetworkReply;
	using ResponseCallBack = std::function<void(QSharedPointer<QtNetworkRequest::ResponseResult> rsp)>;

	class NETWORK_EXPORT NetworkRequestManager : public QObject
	{
		Q_OBJECT
		Q_DECLARE_PRIVATE(NetworkRequestManager)

	public:
		// Initialization and uninitialization must be called in main thread
		static void initialize();
		static void unInitialize();
		// Whether initialized
		static bool isInitialized();

		static NetworkRequestManager *globalInstance();

		// Global proxy (applied to all requests unless overridden per-request)
		static void setGlobalProxy(const ProxyConfig &config);
		static const ProxyConfig &globalProxy();

#ifndef QT_NO_SSL
		// Global SSL/TLS policy (secure default: verify peer + TLS1.2+ + system CA + never ignore errors).
		// Thread-safe. globalSslConfig() returns by value to avoid holding a reference across threads.
		static void setGlobalSslConfig(const SslConfig &config);
		static SslConfig globalSslConfig();
#endif

		// Persistent cookie jar
		static void setCookieStoragePath(const QString &path);
		static QString cookieStoragePath();
		static QNetworkCookieJar *cookieJar();

		// Acquire a thread-affine QNetworkAccessManager from the pool.
		// Must be called from a worker thread (or main thread during
		// initialization).  The returned NAM must NOT be deleted by the
		// caller — its lifetime is managed by the pool.
		static QNetworkAccessManager *acquireThreadNam();

		// Destroy the calling worker thread's NAM if the pool is shutting
		// down. Called from NetworkRequestRunnable::run() exit and from
		// shutdown cleanup QRunnables so that each NAM is deleted on the
		// same thread that created it (QObject affinity — cross-thread
		// delete crashes in the NAM/cookie-jar destructor).
		static void releaseThreadNamOnExit();

	public:
		// Asynchronously execute single request task (returns nullptr if url is invalid)
		std::shared_ptr<NetworkReply> postRequest(std::unique_ptr<RequestContext> context);

		// Asynchronously execute batch request tasks (requests in same batch will be bound to same NetworkReply)
		std::shared_ptr<NetworkReply> postBatchRequest(BatchRequestPtrTasks&& tasks, quint64 &uiBatchId);

		// Synchronously execute single request task (returns false if url is invalid or no idle thread to handle)
		// By default, synchronous mode blocks user interaction to avoid callback object not existing during callback. If set to non-blocking, caller needs to ensure callback lifecycle
		bool sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool bBlockUserInteraction = true);

		// Stop all request tasks (async requests only)
		void stopAllRequest();
		// Stop batch request tasks with specified batchid (async requests only)
		void stopBatchRequests(quint64 uiBatchId);
		// Stop specific request task (async requests only)
		void stopRequest(quint64 uiTaskId);
		// Stop all requests of specific session (async requests only)
		void stopSessionRequest(quint64 uiSessionId);

	public:
		// Set maximum thread count for thread pool (1-100, default is system CPU core count)
		bool setMaxThreadCount(int iMax);
		int maxThreadCount();

		quint64 nextSessionId();

	Q_SIGNALS:
		void errorMessage(const QString &error);
		void batchRequestFinished(quint64 uiBatchId, bool bAllSuccess);

	public Q_SLOTS:
		void onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp);
		// Destroys a runnable on the main thread once its run() has fully
		// returned (see NetworkRequestRunnable::finished). Sole owner of the
		// runnable's lifetime end for cancelled-while-running requests.
		void onRunnableFinished(quint64 uiRequestId);

	public:
		bool event(QEvent *pEvent) Q_DECL_OVERRIDE;

	private:
		explicit NetworkRequestManager(QObject *parent = 0);
		~NetworkRequestManager();
		Q_DISABLE_COPY(NetworkRequestManager);

	private:
		void init();
		void fini();

		bool startAsRunnable(std::unique_ptr<RequestContext> request);

		// bDownload(false: upload)
		void updateProgress(quint64 uiRequestId, quint64 uiBatchId,
							qint64 iBytes, qint64 iTotalBytes, bool bDownload);

	private:
		QScopedPointer<NetworkRequestManagerPrivate> d_ptr;

		static std::atomic<bool> ms_bIntialized;
		static std::atomic<bool> ms_bUnIntializing;
		static ProxyConfig ms_globalProxy;
		static QScopedPointer<QNetworkCookieJar> ms_spCookieJar;
#ifndef QT_NO_SSL
		static SslConfig ms_globalSslConfig;
		static QMutex ms_globalSslConfigMutex;
#endif
	};
}
