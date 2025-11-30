#pragma once

#include <QObject>
#include <memory>
#include <QTimer>

#include "networkrequest.h"

#ifndef QT_NO_SSL
#include <QSslError>
#endif

class QFile;

namespace QtNetworkRequest
{
	// Download request
	class NetworkDownloadRequest : public NetworkRequest
	{
		Q_OBJECT;

	public:
		explicit NetworkDownloadRequest(QObject *parent = 0);
		~NetworkDownloadRequest();

	public Q_SLOTS:
		void start() Q_DECL_OVERRIDE;
		void onFinished() Q_DECL_OVERRIDE;
		void onReadyRead();
		void onDownloadProgress(qint64 iReceived, qint64 iTotal);

	private slots:
#ifndef QT_NO_SSL
		void onSslErrors(const QList<QSslError> &errors);
#endif

	private:
		void CloseFile(bool bRemove);

	private:
		std::unique_ptr<QFile> m_pFile;
		QTimer m_timer;
		int m_mIntervalMs{ 250 };
		bool m_bTimeout = false;
	};
}
