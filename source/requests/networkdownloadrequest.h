#pragma once

#include <QObject>
#include <memory>

#include "networkrequest.h"
#include "progressthrottle.h"

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

	protected:
		void cleanupForRetry() Q_DECL_OVERRIDE { CloseFile(true); }

	private:
		void CloseFile(bool shouldRemove);

	private:
		std::unique_ptr<QFile> m_file;
		std::unique_ptr<ProgressThrottle> m_throttle;
		qint64 m_bytesWritten{ 0 };
	};
}
