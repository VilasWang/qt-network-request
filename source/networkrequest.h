#ifndef NETWORKREQUEST_H
#define NETWORKREQUEST_H
#pragma once

#include <QObject>
#include <memory>
#include <QNetworkReply>
#include "networkrequestdefs.h"
#include <QSharedPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>

class QNetworkAccessManager;
namespace QtNetworkRequest
{
	class NetworkRequest : public QObject
	{
		Q_OBJECT

	public:
		explicit NetworkRequest(QObject *parent = 0);
		virtual ~NetworkRequest();

		const QString errorString() const { return m_strError; }

		void setRequestContext(std::unique_ptr<RequestContext> context);

	protected:
		QSharedPointer<ResponseResult> ToFailedResult(int statusCode = 0, const QByteArray& body = QByteArray(), const QMap<QByteArray, QByteArray>& headers = {});

		QSharedPointer<ResponseResult> ToSuccessResult(const QByteArray& body, const QMap<QByteArray, QByteArray>& headers, int statusCode = 0);
		void applyProxyConfig(QNetworkAccessManager* mgr);
		void applyCookieJar(QNetworkAccessManager* mgr);

		// 重试: 返回 true 表示重试已调度，调用方应直接 return
		bool tryRetry();
		// 子类重写以清理请求特有资源（如文件句柄）
		virtual void cleanupForRetry();
		// 判断错误是否可重试（瞬态错误）
		bool isTransientError(QNetworkReply::NetworkError err);

	public Q_SLOTS:
		virtual void start();
		virtual void abort();
		virtual void onFinished() = 0;
		virtual void onError(QNetworkReply::NetworkError);
		virtual void onAuthenticationRequired(QNetworkReply *, QAuthenticator *);

	Q_SIGNALS:
		void response(QSharedPointer<QtNetworkRequest::ResponseResult> spResult);
		void aboutToAbort();

	protected:
		std::unique_ptr<RequestContext> m_upContext;
		QSharedPointer<ResponseResult> m_spResult;
		bool m_bAbortManual;
		QString m_strError;
		int m_nProgress;
		int m_nRetryCount{ 0 };
		qint64 m_nBytesReceived{ 0 };
		qint64 m_nBytesSent{ 0 };
		quint16 m_nRedirectionCount;
		QNetworkAccessManager *m_pNetworkManager;
		QNetworkReply *m_pNetworkReply;
        QUrl m_url;

		// Layer3: Idle/stall timeout (heartbeat-based)
		QTimer m_heartbeatTimer;          // 250ms periodic heartbeat
		int m_idleTimeoutCount{ 0 };      // consecutive idle periods
		int m_idleThreshold{ 0 };         // threshold = idleTimeoutMs / 250
		void resetIdleTimer();            // call on data arrival to reset idle counter
		virtual void onHeartbeat();       // heartbeat callback (Layer3 idle + Layer2b transfer)

#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
		QElapsedTimer m_transferElapsed;  // Layer2b: transfer timeout timer for Qt < 5.15
#endif
	};

	// Factory class
	class NetworkRequestFactory
	{
	public:
		/// Create request object based on type
		static std::unique_ptr<NetworkRequest> create(std::unique_ptr<RequestContext> context);
	};
}

inline bool isHttpProxy(const QString &strScheme) { return (strScheme.compare(QString("http"), Qt::CaseInsensitive) == 0); }
inline bool isHttpsProxy(const QString &strScheme) { return (strScheme.compare(QString("https"), Qt::CaseInsensitive) == 0); }
inline bool isFtpProxy(const QString &strScheme) { return (strScheme.compare(QString("ftp"), Qt::CaseInsensitive) == 0); }

#endif // NETWORKREQUEST_H
