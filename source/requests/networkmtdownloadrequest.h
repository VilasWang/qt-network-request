#ifndef NETWORKMTDOWNLOADREQUEST_H
#define NETWORKMTDOWNLOADREQUEST_H
#pragma once

#include <QObject>
#include <QPointer>
#include <QMutex>
#include <QElapsedTimer>

#include "networkrequest.h"
#include "memorymappedfile.h"
#include "progressthrottle.h"

class QFile;

namespace QtNetworkRequest
{
	class Downloader;
	class IMDTDownloadState;
	class ProbeState;
	class RangeProbeState;
	class MultiDownloadState;

	// Multi-threaded download request (here thread refers to download channel. A file is divided into multiple parts, downloaded simultaneously by multiple download channels)
	class NetworkMTDownloadRequest : public NetworkRequest
	{
		Q_OBJECT;

		// Phase states access internal members for their phase-specific logic
		friend class ProbeState;
		friend class RangeProbeState;
		friend class MultiDownloadState;

	public:
		explicit NetworkMTDownloadRequest(QObject *parent = 0);
		~NetworkMTDownloadRequest();

		/// Transition to a new phase state. The old state is destroyed.
		void transitionTo(std::unique_ptr<IMDTDownloadState> newState);

	public Q_SLOTS:
		void start() Q_DECL_OVERRIDE;
		void abort() Q_DECL_OVERRIDE;
		void onFinished() Q_DECL_OVERRIDE;
		void onError(QNetworkReply::NetworkError code) Q_DECL_OVERRIDE;
		void onSubPartFinished(int index, bool success, const QString &strErr);
		void onSubPartDownloadProgress(int index, qint64 bytesReceived, qint64 bytesTotal);

	protected:
		/// MTDownload uses state-machine based failure handling (handleProbeFinished etc.)
		/// and must never call the base handleFailure() which would corrupt the state via start().
		bool handleFailure() override { return false; }

	protected:
		void cleanupForRetry() Q_DECL_OVERRIDE { clearDownloaders(); clearProgress(); }

	private:
		// Phase methods called by state objects
		void doProbeRequest();
		void handleProbeFinished(QNetworkReply* reply);
		void doRangeProbeRequest();
		void handleRangeProbeFinished(QNetworkReply* reply);
		void doMultiDownload();

		void startMTDownloadInternal();
		void clearDownloaders();
		void clearProgress();
		QString generateTempFilePath(const QString& originalPath);
		bool renameTempFileToFinal();

	private:
		QString m_dstFilePath;
		QString m_tempFilePath;
		qint64 m_fileSize;

		std::map<int, std::unique_ptr<Downloader>> m_downloaders;
		int m_threadCount; // How many segments to divide into for download
		int m_successCount;
		int m_failedCount;
		QSet<int> m_finishedPartIds;

		std::unique_ptr<MemoryMappedFile> m_mappedFile; // Memory mapped file
		QElapsedTimer m_downloadTimer;					// Download timer

		QMap<int, qint64> m_bytesReceivedByPart;
		qint64 m_bytesTotal;
		QMap<QByteArray, QByteArray> m_responseHeaders;  // Cached HEAD response headers

		bool m_rangeSupportProbed{ false };  // Whether we've completed a range probe
		bool m_rangeSupported{ false };      // Whether the server actually honors Range requests

		std::unique_ptr<IMDTDownloadState> m_state;  // Current phase state
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
							int transferTimeout = 0,
#ifndef QT_NO_SSL
							const SslConfig *sslConfig = nullptr,
#endif
							QObject *parent = 0);

		virtual ~Downloader();

		bool start(const QUrl &url, qint64 startPoint = 0, qint64 endPoint = -1);

		void abort();

		QString errorString() const { return m_errorMessage; }

	Q_SIGNALS:
		void downloadFinished(int index, bool success, const QString &strErr);
		void downloadProgress(int index, qint64 bytesReceived, qint64 bytesTotal);
		void dataReceived();  // N6: fired on readyRead/downloadProgress for Layer3 idle timeout forwarding

	public Q_SLOTS:
		void onFinished();
		void onReadyRead();
		void onError(QNetworkReply::NetworkError code);
#ifndef QT_NO_SSL
		void onSslErrors(const QList<QSslError> &errors);
#endif

	private:
		QPointer<QNetworkAccessManager> m_networkManager;
		QNetworkReply *m_networkReply;
		QUrl m_url;
		bool m_abortManual;
		QString m_errorMessage;

		const int m_index;
		qint64 m_startPoint;
		qint64 m_endPoint;

		bool m_showProgress;
		quint16 m_redirectionCount;
		quint16 m_maxRedirectionCount;

		QPointer<MemoryMappedFile> m_mappedFile; // Memory mapped file pointer
		qint64 m_bytesWritten;					 // Bytes written
		bool m_overflowLogged{ false };		 // Only log overflow once per download
		int m_transferTimeout{ 0 };              // Per-request transfer timeout (ms)
#ifndef QT_NO_SSL
		const SslConfig *m_perRequestSslConfig{ nullptr };
		SslConfig::IgnorePolicy m_ignorePolicy{ SslConfig::IgnorePolicy::Never };
		QList<QSslError::SslError> m_resolvedIgnoreErrorTypes;
#endif

		std::unique_ptr<ProgressThrottle> m_throttle;
	};
}

#endif // NETWORKMTDOWNLOADREQUEST_H
