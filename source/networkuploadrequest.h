#pragma once

#include <QObject>
#include <QTimer>

#include "networkrequest.h"

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

	private:
		void CloseFile();

	private:
		std::unique_ptr<QFile> m_pFile;
		QTimer m_timer;
		int m_mIntervalMs{ 250 };
		bool m_bTimeout = false;
	};
}
