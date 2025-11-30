#ifndef NETWORKREQUEST_H
#define NETWORKREQUEST_H
#pragma once

#include <QObject>
#include <memory>
#include <QNetworkReply>
#include "networkrequestdefs.h"
#include <QSharedPointer>

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
		QSharedPointer<ResponseResult> ToFailedResult(const QByteArray& body = QByteArray(), const QMap<QByteArray, QByteArray>& headers = {});
		QSharedPointer<ResponseResult> ToSuccessResult(const QByteArray& body, const QMap<QByteArray, QByteArray>& headers);

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
		quint16 m_nRedirectionCount;
		QNetworkAccessManager *m_pNetworkManager;
		QNetworkReply *m_pNetworkReply;
        QUrl m_url;
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
