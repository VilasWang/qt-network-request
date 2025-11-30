#ifndef NETWORKBIGFLEDOWNLOADREQUEST_H
#define NETWORKBIGFLEDOWNLOADREQUEST_H
#pragma once

#include <QObject>
#include <QPointer>
#include <QMutex>
#include <QElapsedTimer>
#include <QTimer>

#include "networkrequest.h"
#include "memorymappedfile.h"

class QFile;

namespace QtNetworkRequest
{
	class Downloader;

	// Multi-threaded download request (here thread refers to download channel. A file is divided into multiple parts, downloaded simultaneously by multiple download channels)
	class NetworkMTDownloadRequest : public NetworkRequest
	{
		Q_OBJECT;

	public:
		explicit NetworkMTDownloadRequest(QObject *parent = 0);
		~NetworkMTDownloadRequest();

	public Q_SLOTS:
		void start() Q_DECL_OVERRIDE;
		void abort() Q_DECL_OVERRIDE;
		void onFinished() Q_DECL_OVERRIDE;
		void onSubPartFinished(int index, bool bSuccess, const QString &strErr);
		void onSubPartDownloadProgress(int index, qint64 bytesReceived, qint64 bytesTotal);

	private:
		bool requestFileSize();
		void startMTDownload();
		void clearDownloaders();
		void clearProgress();
		QString generateTempFilePath(const QString& originalPath);
		bool renameTempFileToFinal();

	private:
		QString m_strDstFilePath;
		QString m_strTempFilePath;
		qint64 m_nFileSize;

		std::map<int, std::unique_ptr<Downloader>> m_mapDownloader;
		int m_nThreadCount; // How many segments to divide into for download
		int m_nSuccess;
		int m_nFailed;
		QSet<int> m_setFinishedIds;

		std::unique_ptr<MemoryMappedFile> m_mappedFile; // Memory mapped file
		QElapsedTimer m_downloadTimer;					// Download timer

		QMap<int, qint64> m_mapBytesReceived;
		qint64 m_bytesTotal;
	};

	// Used for downloading files (or part of a file)
	class Downloader : public QObject
	{
		Q_OBJECT

	public:
		explicit Downloader(int index,
							MemoryMappedFile *mappedFile,
							QNetworkAccessManager *pNetworkManager,
							bool bShowProgress = false,
							quint16 nMaxRedirectionCount = 5,
							QObject *parent = 0);

		virtual ~Downloader();

		bool start(const QUrl &url, qint64 startPoint = 0, qint64 endPoint = -1);

		void abort();

		QString errorString() const { return m_strError; }

	Q_SIGNALS:
		void downloadFinished(int index, bool bSuccess, const QString &strErr);
		void downloadProgress(int index, qint64 bytesReceived, qint64 bytesTotal);

	public Q_SLOTS:
		void onFinished();
		void onReadyRead();
		void onError(QNetworkReply::NetworkError code);

	private:
		QPointer<QNetworkAccessManager> m_pNetworkManager;
		QNetworkReply *m_pNetworkReply;
		QUrl m_url;
		bool m_bAbortManual;
		QString m_strError;

		const int m_nIndex;
		qint64 m_nStartPoint;
		qint64 m_nEndPoint;

		bool m_bShowProgress;
		quint16 m_nRedirectionCount;
		quint16 m_nMaxRedirectionCount;

		QPointer<MemoryMappedFile> m_mappedFile; // Memory mapped file pointer
		qint64 m_bytesWritten;					 // Bytes written

		QTimer m_timer;
		int m_mIntervalMs{ 250 };
		bool m_bTimeout = false;
	};
}

#endif // NETWORKBIGFLEDOWNLOADREQUEST_H
