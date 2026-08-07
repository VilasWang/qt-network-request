#ifndef NETWORKREQUEST_H
#define NETWORKREQUEST_H
#pragma once

#include <QObject>
#include <memory>
#include <QNetworkReply>
#include "requestcontext.h"
#include "responseresult.h"
#include <QSharedPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include "qtcompat.h"
#ifndef QT_NO_SSL
#include <QSslError>
#endif

class QNetworkAccessManager;
namespace QtNetworkRequest
{
	class NetworkRequest : public QObject
	{
		Q_OBJECT

	public:
		explicit NetworkRequest(QObject *parent = 0);
		virtual ~NetworkRequest();

		const QString errorString() const { return m_errorMessage; }

		void setRequestContext(std::unique_ptr<RequestContext> context);

		// 判断错误是否可重试（瞬态错误）。static 以便策略类复用，避免重复定义。
		static bool isTransientError(QNetworkReply::NetworkError err);

#ifndef QT_NO_SSL
		// Merge global + per-request SSL config into an effective configuration.
		static SslConfig resolveSslConfig(const SslConfig *perRequest);
		// Apply an already-resolved SslConfig to a QNetworkRequest.
		static void applySslConfigToRequest(QNetworkRequest &request, const SslConfig &resolved);
#endif

	protected:
		QSharedPointer<ResponseResult> ToFailedResult(int statusCode = 0, const QByteArray& body = QByteArray(), const QMap<QByteArray, QByteArray>& headers = {});

		QSharedPointer<ResponseResult> ToSuccessResult(const QByteArray& body, const QMap<QByteArray, QByteArray>& headers, int statusCode = 0);
		void applyProxyConfig(QNetworkAccessManager* mgr);
		void applyCookieJar(QNetworkAccessManager* mgr);
#ifndef QT_NO_SSL
		// Resolve (global + per-request) and apply SSL config to a QNetworkRequest.
		// Caches the resolved ignore policy for onSslErrors(). Call before sending.
		void applySslConfig(QNetworkRequest &request);
		// Connect reply's sslErrors signal to this request's onSslErrors slot.
		void connectSslErrorHandling(QNetworkReply *reply);
#endif

		// 设置结构化错误 (同时同步 m_error 与 m_errorMessage 消息)
		void setError(ErrorCategory category, ErrorCode code, const QString& msg, int nativeCode = 0);

		// 重试: 返回 true 表示重试已调度，调用方应直接 return
		bool tryRetry();
		// 子类重写以清理请求特有资源（如文件句柄）
		virtual void cleanupForRetry();

		// ── Shared helper methods (consolidated from duplicated subclass code) ──

		/// Acquire NAM, set cookies, return a pre-configured QNetworkRequest.
		/// Subclasses call this at the start of their start() to avoid duplicating
		/// the NAM acquisition + proxy + cookie + header + SSL + transferTimeout setup.
		QNetworkRequest prepareRequest();

		/// Check success: NoError + HTTP proxy 2xx.  @return (success, httpStatusCode)
		std::pair<bool, int> evaluateOutcome();

		/// Attempt retry or redirect.  @return true if the caller should return immediately
		/// (retry was scheduled or redirect restarted the request).
		/// Virtual: MTDownloadRequest overrides to prevent state-machine corruption on redirect.
		virtual bool handleFailure();

		/// Populate responseHeaders + read body from reply into result.
		void collectResponse(QMap<QByteArray, QByteArray>& outHeaders, QByteArray& outBody);

		/// Apply auth-generated headers to the request.
		/// User-set headers with the same name take priority (not overwritten).
		void applyAuthConfig(QNetworkRequest &request);

		/// Auto-set Content-Type header based on BodyType when not explicitly set by user.
		void applyBodyTypeContentType(QNetworkRequest &request);

		/// Returns the effective request body as QByteArray.
		/// Uses binaryBody for BodyType::Binary, otherwise body.toUtf8().
		QByteArray effectiveRequestBody() const;

		/// Cleanup reply with deleteLater() and null the pointer.
		void disposeReply();

		// ── Strategy hooks (for future strategy-pattern injection) ──
		// Currently unused — reserved for Phase 3 full migration
		// virtual QNetworkRequest  buildRequest()      = 0;
		// virtual QNetworkReply*  executeRequest(QNetworkRequest&) = 0;
		// virtual bool            processResponseBody(QNetworkReply*) { return true; }

	public Q_SLOTS:
		virtual void start();
		virtual void abort();
		virtual void onFinished() = 0;
		virtual void onError(QNetworkReply::NetworkError);
		virtual void onAuthenticationRequired(QNetworkReply *, QAuthenticator *);
#ifndef QT_NO_SSL
		virtual void onSslErrors(const QList<QSslError> &errors);
#endif

	Q_SIGNALS:
		void response(QSharedPointer<QtNetworkRequest::ResponseResult> spResult);
		void aboutToAbort();

	protected:
		std::unique_ptr<RequestContext> m_context;
		QSharedPointer<ResponseResult> m_result;
		bool m_abortManual;
		QString m_errorMessage;
		ErrorInfo m_error;
		int m_progress;
		int m_retryCount{ 0 };
		qint64 m_bytesReceived{ 0 };
		qint64 m_bytesSent{ 0 };
		quint16 m_redirectionCount;
		bool m_oauthRefreshed{ false };   // M2: guard against infinite refresh loops
		QNetworkAccessManager *m_networkManager;  // non-owning — managed by NetworkAccessManagerPool
		QNetworkReply *m_networkReply;
        QUrl m_url;
#ifndef QT_NO_SSL
		SslConfig::IgnorePolicy m_resolvedIgnorePolicy{ SslConfig::IgnorePolicy::Never };
		QList<QSslError::SslError> m_resolvedIgnoreErrorTypes;
#endif

		// Layer3: Idle/stall timeout (heartbeat-based)
		QTimer m_heartbeatTimer;          // 250ms periodic heartbeat
		int m_idleTimeoutCount{ 0 };      // consecutive idle periods
		int m_idleThreshold{ 0 };         // threshold = idleTimeoutMs / 250
		void resetIdleTimer();            // call on data arrival to reset idle counter
		virtual void onHeartbeat();       // heartbeat callback (Layer3 idle + Layer2b transfer)

		// Layer2b: transfer timeout timer.
		// On Qt >= 5.15 setTransferTimeout() is used; on < 5.15 heartbeat-based.
		QElapsedTimer m_transferElapsed;
	};

}

inline bool isHttpProxy(const QString &strScheme) { return (strScheme.compare(QString("http"), Qt::CaseInsensitive) == 0); }
inline bool isHttpsProxy(const QString &strScheme) { return (strScheme.compare(QString("https"), Qt::CaseInsensitive) == 0); }
inline bool isFtpProxy(const QString &strScheme) { return (strScheme.compare(QString("ftp"), Qt::CaseInsensitive) == 0); }

#endif // NETWORKREQUEST_H
