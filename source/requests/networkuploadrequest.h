#pragma once

#include <QObject>

#include "networkrequest.h"
#include "progressthrottle.h"

class QFile;

namespace QtNetworkRequest
{
	// Upload request
	class NetworkUploadRequest : public NetworkRequest
	{
		Q_OBJECT;

	public:
		explicit NetworkUploadRequest(QObject *parent = 0);
		~NetworkUploadRequest();

	public Q_SLOTS:
		void start() Q_DECL_OVERRIDE;
		void onFinished() Q_DECL_OVERRIDE;
		void onUploadProgress(qint64, qint64);

	protected:
		void cleanupForRetry() Q_DECL_OVERRIDE { CloseFile(); }

	private:
		void CloseFile();

	private:
		std::unique_ptr<QFile> m_file;
		std::unique_ptr<ProgressThrottle> m_throttle;
		qint64 m_lastSentBytes{ 0 };
	};
}
