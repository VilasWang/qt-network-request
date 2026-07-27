#include "networkmtdownloadrequest.h"
#include "networkmtdownloadrequest_p.h"
#include "memorymappedfile.h"
#include "qtcompat.h"
#include "networkrequestregistry.h"
#include <QThread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#endif
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QUuid>
#include "networkrequestmanager.h"
#include "networkrequestutility.h"
#include "networkrequestevent.h"

// Self-registration: multi-thread download (higher priority than single-thread),
// also for explicit MTDownload type.
namespace {
	[[maybe_unused]] static const int _regMTDownload = []() -> int {
		using namespace QtNetworkRequest;
		// MTDownload type — explicit multi-thread download
		NetworkRequestRegistry::instance().registerCreator(
			RequestType::MTDownload, 10,
			[](RequestContext*) -> NetworkRequest* {
				return new NetworkMTDownloadRequest();
			});
		// Download type with threadCount > 1 (or auto) — higher priority than single-thread
		NetworkRequestRegistry::instance().registerCreator(
			RequestType::Download, 10,
			[](RequestContext* ctx) -> NetworkRequest* {
				if (ctx && ctx->downloadConfig && (ctx->downloadConfig->threadCount > 1 || ctx->downloadConfig->threadCount == 0))
					return new NetworkMTDownloadRequest();
				return nullptr; // fall through to single-thread DownloadRequest
			});
		return 0;
	}();
}

using namespace QtNetworkRequest;

NetworkMTDownloadRequest::NetworkMTDownloadRequest(QObject *parent /* = nullptr */)
    : NetworkRequest(parent), m_nThreadCount(0), m_nSuccess(0), m_nFailed(0), m_bytesTotal(0), m_nFileSize(-1)
{
}

NetworkMTDownloadRequest::~NetworkMTDownloadRequest()
{
    abort(); // Stop all download tasks
}

void NetworkMTDownloadRequest::abort()
{
    NetworkRequest::abort();
    clearDownloaders();

    // Close memory mapped file
    if (m_mappedFile)
    {
        m_mappedFile->close();
        m_mappedFile.reset();
    }

    // Clean up temporary file if it exists
    if (!m_strTempFilePath.isEmpty())
    {
        QFile tempFile(m_strTempFilePath);
        if (tempFile.exists())
        {
            tempFile.remove();
        }
        m_strTempFilePath.clear();
    }

    clearProgress();
}

void NetworkMTDownloadRequest::transitionTo(std::unique_ptr<IMDTDownloadState> newState)
{
	qDebug() << "[MTDownload] State transition:" << (m_state ? m_state->name() : QStringLiteral("null"))
			 << "→" << (newState ? newState->name() : QStringLiteral("null"));
	m_state = std::move(newState);
	if (m_state)
		m_state->enter(this);
}

void NetworkMTDownloadRequest::start()
{
	NetworkRequest::start();

	m_nSuccess = 0;
	m_nFailed = 0;
	m_nThreadCount = 1;

	// Initialize state machine — ProbeState handles HEAD request and transitions onward
	transitionTo(std::make_unique<ProbeState>());
}

void NetworkMTDownloadRequest::onFinished()
{
	if (m_state && m_pNetworkReply)
		m_state->onFinished(this, m_pNetworkReply);
}

void NetworkMTDownloadRequest::onError(QNetworkReply::NetworkError code)
{
	NetworkRequest::onError(code);
	if (m_state)
		m_state->onError(this, code);
}

void NetworkMTDownloadRequest::startMTDownloadInternal()
{
    if (m_bAbortManual)
    {
        return;
    }

    // Start timing
    m_downloadTimer.start();
    if (m_nFileSize <= 0)
    {
        setError(ErrorCategory::Protocol, ErrorCode::ContentLengthMissing, "Server error: Content-Length header not provided");
        qDebug() << "[QMultiThreadNetwork]" << m_strError;

        emit response(ToFailedResult());
        return;
    }
    m_strDstFilePath = NetworkRequestUtility::getFilePath(m_upContext.get(), m_strError);
    if (m_strDstFilePath.isEmpty())
    {
        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, m_strError);
        emit response(ToFailedResult());
        return;
    }

    // Generate temporary file path
    m_strTempFilePath = generateTempFilePath(m_strDstFilePath);
    if (m_strTempFilePath.isEmpty())
    {
        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, "Failed to generate temporary file path");
        emit response(ToFailedResult());
        return;
    }

    if (m_bAbortManual)
    {
        return;
    }

    // Create and open memory mapped file with temporary name
    m_mappedFile = std::make_unique<MemoryMappedFile>();
    if (!m_mappedFile->open(m_strTempFilePath, m_nFileSize))
    {
        setError(ErrorCategory::FileIo, ErrorCode::MemoryMappingFailed,
                 QString("Memory mapping error: Failed to create memory mapped file - %1").arg(m_mappedFile->lastError()));
        qDebug() << "[QMultiThreadNetwork]" << m_strError;
        emit response(ToFailedResult());
        return;
    }
    clearDownloaders();
    Q_ASSERT(nullptr != m_upContext->downloadConfig);
    m_nThreadCount = m_upContext->downloadConfig->threadCount;
    // If threadCount is 0, auto detect CPU cores
    if (m_nThreadCount == 0) {
        m_nThreadCount = QThread::idealThreadCount();
        qDebug() << "[QMultiThreadNetwork]" << "Auto-detected thread count:" << m_nThreadCount;
    }
    // Enforce a minimum of 2 threads only when the server actually honors
    // Range requests.  When the range probe returned 200 (not 206), or the
    // server doesn't advertise Accept-Ranges at all, we already forced
    // threadCount = 1 and must keep it — splitting into multiple
    // Range-based parts would re-create the overflow.
    if (!m_bRangeSupportProbed || m_bRangeSupported)
        m_nThreadCount = qMax(m_nThreadCount, 2);
    m_bytesTotal = m_nFileSize;

    // Divide file into n segments and download asynchronously
    for (int i = 0; i < m_nThreadCount; i++)
    {
        qint64 start = 0;
        qint64 end = -1;
        if (m_nThreadCount > 1)
        {
            // First calculate the start and end of each segment (information required by HTTP protocol)
            start = m_nFileSize * i / m_nThreadCount;
            end = m_nFileSize * (i + 1) / m_nThreadCount;
        }
        if (m_nThreadCount == i + 1)
        {
            end = m_nFileSize - 1;
        }
        // Download the file in segments
        std::unique_ptr<Downloader> downloader = 
            std::make_unique<Downloader>(i, 
                m_mappedFile.get(), 
                m_pNetworkManager, 
                m_upContext->behavior.showProgress, 
                m_upContext->behavior.maxRedirectionCount, 
                m_upContext->behavior.transferTimeout,
#ifndef QT_NO_SSL
                m_upContext->sslConfig.get(),
#else
                nullptr,
#endif
                this);

        connect(downloader.get(), SIGNAL(downloadFinished(int, bool, const QString &)),
                this, SLOT(onSubPartFinished(int, bool, const QString &)));
        connect(downloader.get(), SIGNAL(downloadProgress(int, qint64, qint64)),
                this, SLOT(onSubPartDownloadProgress(int, qint64, qint64)));
        // N6: Forward Downloader data arrival to Layer3 idle timeout
        connect(downloader.get(), &Downloader::dataReceived,
                this, &NetworkMTDownloadRequest::resetIdleTimer);
        if (downloader->start(m_upContext->url, start, end))
        {
            m_mapDownloader[i] = std::move(downloader);
            m_mapBytesReceived.insert(i, 0);
        }
        else
        {
            abort();
            setError(ErrorCategory::Network, ErrorCode::Unknown,
                     QString("Download error: Part %1 failed - %2").arg(i).arg(downloader->errorString()));
            emit response(ToFailedResult());
            return;
        }
    }
}

void NetworkMTDownloadRequest::onSubPartFinished(int index, bool bSuccess, const QString &strErr)
{
    if (m_bAbortManual)
    {
        return;
    }
    if (m_setFinishedIds.contains(index))
    {
        qDebug() << "[QMultiThreadNetwork] Download repeated part finished.";
        return;
    }
    m_setFinishedIds.insert(index);

    if (bSuccess)
    {
        m_nSuccess++;
    }
    else
    {
        if (++m_nFailed == 1)
        {
            abort();
        }
        if (m_strError.isEmpty())
        {
            setError(ErrorCategory::Network, ErrorCode::Unknown, strErr);
        }
    }

    // If completion count equals file segment count, file download is successful; if failure count > 0, download failed
    if (m_nSuccess == m_nThreadCount || m_nFailed > 0)
    {
        if (m_nFailed == 0)
        {
            // Record download end time and elapsed time
            qint64 elapsedMs = m_downloadTimer.elapsed();
            double elapsedSeconds = elapsedMs / 1000.0;

            // Get response header information from the saved HEAD response
            // (m_pNetworkReply is already nullptr at this point because the
            //  HEAD reply was deleted in onFinished() and range-probe reply
            //  was deleted in onRangeProbeFinished()).
            QMap<QByteArray, QByteArray> responseHeaders = m_responseHeaders;
            // Close memory mapped file before rename operation
            if (m_mappedFile)
            {
                m_mappedFile->close();
                m_mappedFile.reset();
            }

            // Rename temporary file to final name
            if (!renameTempFileToFinal())
            {
                setError(ErrorCategory::FileIo, ErrorCode::FileRenameFailed,
                         QString("Failed to rename temporary file to final destination: %1").arg(m_strError));
                emit response(ToFailedResult());
                return;
            }

            m_nBytesReceived = m_nFileSize;
            if (m_spResult)
                m_spResult->performance.bytesReceived = m_nBytesReceived;

            double speed = (m_nFileSize / 1024.0 / 1024.0) / elapsedSeconds;
            QString msg = QString("The download took %1 seconds in total, with an average speed of %2 MB/s.").arg(elapsedSeconds).arg(speed);
            emit response(ToSuccessResult(msg.toUtf8(), responseHeaders));

            qDebug() << "[QMultiThreadNetwork] Download took " << elapsedSeconds << "seconds (" << elapsedMs << "ms)";
            qDebug() << "[QMultiThreadNetwork] Average speed:" << QString::number(speed, 'f', 2) << "MB/s";
        }
        else
        {
            emit response(ToFailedResult());

            qDebug() << "[QMultiThreadNetwork] Download failed:" << m_strError;
            qDebug() << "[QMultiThreadNetwork] Download failed after" << (m_downloadTimer.elapsed() / 1000.0) << "seconds";
        }
    }
}

void NetworkMTDownloadRequest::onSubPartDownloadProgress(int index, qint64 bytesReceived, qint64 bytesTotal)
{
    // Reset idle timeout on any sub-part data arrival
    if (bytesReceived > 0)
        resetIdleTimer();

    if (m_bAbortManual || bytesReceived <= 0 || bytesTotal <= 0)
        return;

    if (!m_mapBytesReceived.contains(index))
    {
        return;
    }
	// qDebug() << "Part:" << index << " progress:" << bytesReceived << "/" << bytesTotal;
    m_mapBytesReceived[index] = bytesReceived;

	if (m_bytesTotal > 0)
	{
        qint64 totalReceived = 0;
		for (auto iter = m_mapBytesReceived.begin(); iter != m_mapBytesReceived.end(); ++iter)
		{
            totalReceived += iter.value();
        }
		int progress = totalReceived * 100 / m_bytesTotal;
		if (m_nProgress < progress)
		{
			m_nProgress = progress;
			NetworkProgressEvent* event = new NetworkProgressEvent;
			event->uiId = m_upContext->task.id;
			event->uiBatchId = m_upContext->task.batchId;
			event->iBytes = totalReceived;
			event->iTotalBytes = m_bytesTotal;
			QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
		}
	}
}

// ============================================================================
// State-aware phase methods — called by state classes
// ============================================================================

void NetworkMTDownloadRequest::doProbeRequest()
{
	if (!m_url.isValid()) return;
	m_nFileSize = -1;

	if (nullptr == m_pNetworkManager)
		m_pNetworkManager = NetworkRequestManager::acquireThreadNam();

	applyProxyConfig(m_pNetworkManager);

	QNetworkRequest request(m_url);
	QtCompat::setTransferTimeout(request, m_upContext->behavior.transferTimeout);
#ifndef QT_NO_SSL
	applySslConfig(request);
#endif

	m_pNetworkReply = m_pNetworkManager->head(request);
	if (m_pNetworkReply)
	{
		connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
		QtCompat::connectErrorSignal(m_pNetworkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
		connectSslErrorHandling(m_pNetworkReply);
#endif
	}
	QtCompat::startTransferTimer(m_transferElapsed);
}

void NetworkMTDownloadRequest::handleProbeFinished(QNetworkReply* reply)
{
	if (!reply)
	{
		setError(ErrorCategory::Network, ErrorCode::InvalidReply, QString("Network error: Invalid reply"));
		emit response(ToFailedResult());
		return;
	}

	auto [bSuccess, statusCode] = evaluateOutcome();
	if (!bSuccess)
	{
		if (statusCode == 301 || statusCode == 302)
		{
			const QVariant& redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
			const QUrl& redirectUrl = m_url.resolved(redirectionTarget.toUrl());
			if (redirectUrl.isValid() && m_url != redirectUrl &&
				++m_nRedirectionCount <= m_upContext->behavior.maxRedirectionCount)
			{
				qDebug() << "[MTDownload] HEAD redirect:" << m_url.toString() << "→" << redirectUrl.toString();
				m_url = redirectUrl;
				reply->deleteLater();
				m_pNetworkReply = nullptr;
				doProbeRequest();
				return;
			}
		}
		else
		{
			bool bHttpProxy = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
			if (bHttpProxy)
				qDebug() << "[MTDownload]" << QString("HTTP error: status code %1").arg(statusCode);
		}

		if (tryRetry())
			return;
		m_strError = QString("HTTP error: Failed to retrieve file size - Status code %1").arg(statusCode);
		qDebug() << "[MTDownload]" << m_strError;
		emit response(ToFailedResult(statusCode));
		return;
	}

	clearProgress();

	// Read Content-Length
	const QVariant& var = reply->header(QNetworkRequest::ContentLengthHeader);
	m_nFileSize = var.toLongLong();
	m_bytesTotal = m_nFileSize;
	qDebug() << "[MTDownload] File size:" << m_nFileSize;

	// Check Accept-Ranges
	QByteArray acceptRanges = reply->rawHeader("Accept-Ranges");
	bool serverClaimsRange = acceptRanges.toLower().contains("bytes");

	// Preserve response headers
	m_responseHeaders.clear();
	foreach (const QByteArray& header, reply->rawHeaderList())
		m_responseHeaders[header] = reply->rawHeader(header);
	m_responseHeaders["X-Final-Url"] = m_url.toString().toUtf8();

	reply->deleteLater();
	m_pNetworkReply = nullptr;

	if (serverClaimsRange && m_nFileSize > 0)
	{
		transitionTo(std::make_unique<RangeProbeState>());
	}
	else
	{
		m_bRangeSupportProbed = true;
		m_bRangeSupported = false;
		qDebug() << "[MTDownload] Server does not advertise Accept-Ranges, falling back to single-thread";
		m_upContext->downloadConfig->threadCount = 1;
		transitionTo(std::make_unique<MultiDownloadState>());
	}
}

void NetworkMTDownloadRequest::doRangeProbeRequest()
{
	if (!m_url.isValid() || m_nFileSize <= 0)
	{
		m_upContext->downloadConfig->threadCount = 1;
		transitionTo(std::make_unique<MultiDownloadState>());
		return;
	}

	if (nullptr == m_pNetworkManager)
		m_pNetworkManager = NetworkRequestManager::acquireThreadNam();

	applyProxyConfig(m_pNetworkManager);

	QNetworkRequest request(m_url);
	request.setRawHeader("Range", "bytes=0-0");
	QtCompat::setHttp2Allowed(request, false);
	QtCompat::setTransferTimeout(request, m_upContext->behavior.transferTimeout);
	request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, false);
#ifndef QT_NO_SSL
	applySslConfig(request);
#endif

	qDebug() << "[MTDownload] Probing Range support with bytes=0-0...";
	m_pNetworkReply = m_pNetworkManager->get(request);
	if (m_pNetworkReply)
	{
		connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
#ifndef QT_NO_SSL
		connectSslErrorHandling(m_pNetworkReply);
#endif
		return;
	}

	m_upContext->downloadConfig->threadCount = 1;
	transitionTo(std::make_unique<MultiDownloadState>());
}

void NetworkMTDownloadRequest::handleRangeProbeFinished(QNetworkReply* reply)
{
	if (!reply)
	{
		m_upContext->downloadConfig->threadCount = 1;
		transitionTo(std::make_unique<MultiDownloadState>());
		return;
	}

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	bool probeSuccess = (statusCode == 206);

	m_bRangeSupportProbed = true;
	m_bRangeSupported = probeSuccess;

	qDebug() << "[MTDownload] Range probe result:" << statusCode
			 << (probeSuccess ? "(206 - supported)" : "(not supported)");

	if (!probeSuccess)
		m_upContext->downloadConfig->threadCount = 1;

	reply->deleteLater();
	m_pNetworkReply = nullptr;

	transitionTo(std::make_unique<MultiDownloadState>());
}

void NetworkMTDownloadRequest::doMultiDownload()
{
	startMTDownloadInternal();
}

void NetworkMTDownloadRequest::clearDownloaders()
{
    for (std::pair<const int, std::unique_ptr<Downloader>> &pair : m_mapDownloader)
    {
        if (pair.second.get())
        {
            // First disconnect all signal connections to prevent race conditions caused by async callbacks
            pair.second->disconnect();
            // Then safely stop the download
            pair.second->abort();
        }
    }
    m_mapDownloader.clear();
    m_setFinishedIds.clear();
}

void NetworkMTDownloadRequest::clearProgress()
{
    m_mapBytesReceived.clear();
    m_bytesTotal = 0;
}

QString NetworkMTDownloadRequest::generateTempFilePath(const QString& originalPath)
{
    QFileInfo fileInfo(originalPath);
    QString dirPath = fileInfo.absolutePath();
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();

    // Generate unique temporary file name
    QString uuid = QUuid::createUuid().toString().remove('{').remove('}');
    QString tempName = QString("%1.%2").arg(uuid).arg(suffix.isEmpty() ? "tmp" : suffix);

    return QDir(dirPath).absoluteFilePath(tempName);
}

bool NetworkMTDownloadRequest::renameTempFileToFinal()
{
    if (m_strTempFilePath.isEmpty() || m_strDstFilePath.isEmpty())
    {
        m_strError = "Invalid file paths for rename operation";
        return false;
    }

    QFile tempFile(m_strTempFilePath);
    if (!tempFile.exists())
    {
        m_strError = "Temporary file does not exist";
        return false;
    }

    // Check if final file already exists
    QFile finalFile(m_strDstFilePath);
    if (finalFile.exists())
    {
        // If overwrite is enabled, remove existing file
        if (m_upContext->downloadConfig && m_upContext->downloadConfig->overwriteFile)
        {
            if (!finalFile.remove())
            {
                m_strError = "Failed to remove existing file: " + finalFile.errorString();
                return false;
            }
        }
        else
        {
            m_strError = "Destination file already exists and overwrite is disabled";
            return false;
        }
    }

    // Rename temporary file to final name
    if (!tempFile.rename(m_strDstFilePath))
    {
        m_strError = "Failed to rename file: " + tempFile.errorString();
        return false;
    }

    // Clear temporary file path after successful rename
    m_strTempFilePath.clear();

    return true;
}

//////////////////////////////////////////////////////////////////////////
Downloader::Downloader(int index, MemoryMappedFile *mappedFile, QNetworkAccessManager *pNetworkManager, bool bShowProgress, quint16 nMaxRedirectionCount, int transferTimeout
#ifndef QT_NO_SSL
    , const SslConfig *sslConfig
#endif
    , QObject *parent)
    : QObject(parent),
      m_nIndex(index),
      m_pNetworkReply(nullptr),
      m_bAbortManual(false),
      m_nStartPoint(0),
      m_nEndPoint(0),
      m_nRedirectionCount(0),
      m_pNetworkManager(QPointer<QNetworkAccessManager>(pNetworkManager)),
      m_bShowProgress(bShowProgress),
      m_nMaxRedirectionCount(nMaxRedirectionCount),
      m_mappedFile(QPointer<MemoryMappedFile>(mappedFile)),
      m_bytesWritten(0),
      m_transferTimeout(transferTimeout)
#ifndef QT_NO_SSL
      , m_perRequestSslConfig(sslConfig)
      , m_ignorePolicy(SslConfig::IgnorePolicy::Never)
#endif
{
	m_throttle = std::make_unique<ProgressThrottle>(250, this);
}

Downloader::~Downloader()
{
    abort();
}

void Downloader::abort()
{
    m_bAbortManual = true;
    m_throttle->stop();
    if (m_pNetworkReply)
    {
        if (m_pNetworkReply->isRunning())
        {
            m_pNetworkReply->abort();
        }
        m_pNetworkReply->deleteLater();
        m_pNetworkReply = nullptr;
    }

    // Memory mapped file is managed externally, no need to close here
    m_mappedFile = nullptr;
    m_pNetworkManager = nullptr;
}

bool Downloader::start(const QUrl &url, qint64 startPoint, qint64 endPoint)
{
    if (nullptr == m_pNetworkManager || nullptr == m_mappedFile || !url.isValid())
    {
        m_strError = QString("Parameter error: Invalid parameters provided");
        return false;
    }

    m_bAbortManual = false;
    m_bytesWritten = 0;
    m_bOverflowLogged = false;

    m_url = url;
    m_nStartPoint = startPoint;
    m_nEndPoint = endPoint;

    // Verify if download range is valid
    if (startPoint < 0 || endPoint < startPoint)
    {
        m_strError = QString("Range error: Invalid download range %1-%2").arg(startPoint).arg(endPoint);
        return false;
    }

    // Check if exceeding file size
    qint64 fileSize = m_mappedFile->size();
    if (startPoint >= fileSize)
    {
        m_strError = QString("Range error: Start point %1 exceeds file size %2").arg(startPoint).arg(fileSize);
        return false;
    }

    if (endPoint >= fileSize)
    {
        endPoint = fileSize - 1;
        m_nEndPoint = endPoint;
    }
    // HTTP Range unit token is case-sensitive and MUST be lowercase "bytes="
    // (RFC 7233). Using "Bytes=" makes the server ignore the Range header and
    // return the full file, causing each thread to download the entire file and
    // the progress to exceed 100%.
    QString range = QString::asprintf("bytes=%lld-%lld", m_nStartPoint, m_nEndPoint);
    if (range.isEmpty())
    {
        m_strError = QString("Range error: Invalid download range specified");
        return false;
    }
    // According to HTTP protocol, write RANGE header to specify file range request
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Range", range.toLocal8Bit());
    QtCompat::setTransferTimeout(request, m_transferTimeout);
    // Force HTTP/1.1 — when HTTP/2 multiplexes concurrent Range requests
    // onto a single connection, CDNs (Cloudflare, Varnish) may drop the
    // Range header and return 200 with the full body.
    QtCompat::setHttp2Allowed(request, false);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, false);

#ifndef QT_NO_SSL
    if (url.scheme().toLower() == "https")
    {
        SslConfig resolved = NetworkRequest::resolveSslConfig(m_perRequestSslConfig);
        m_ignorePolicy = resolved.ignoreSslErrorsPolicy;
        m_resolvedIgnoreErrorTypes = resolved.ignoreErrorTypes;
        NetworkRequest::applySslConfigToRequest(request, resolved);
    }
#endif

    qDebug() << "[QMultiThreadNetwork] Part" << m_nIndex << "Range:" << range;

    m_pNetworkReply = m_pNetworkManager->get(request);
    if (m_pNetworkReply)
    {
        connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
        connect(m_pNetworkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        QtCompat::connectErrorSignal(m_pNetworkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
        connect(m_pNetworkReply, &QNetworkReply::sslErrors, this, &Downloader::onSslErrors);
#endif

        connect(m_pNetworkReply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal)
            {
                m_throttle->report(bytesReceived, bytesTotal, nullptr);
                // N6: Forward data arrival for Layer3 idle timeout
                if (bytesReceived > 0)
                    emit dataReceived();
                emit downloadProgress(m_nIndex, bytesReceived, bytesTotal); 
            });
    }
    m_throttle->start();
    return true;
}

#ifndef QT_NO_SSL
void Downloader::onSslErrors(const QList<QSslError> &errors)
{
    if (m_ignorePolicy == SslConfig::IgnorePolicy::Always)
    {
        qWarning() << "[QMultiThreadNetwork] SSL errors IGNORED (policy=Always) for part"
                   << m_nIndex << m_url.toString() << "- NOT for production use:";
        for (const QSslError &e : errors)
            qWarning() << "   " << e.errorString();
        if (m_pNetworkReply)
            m_pNetworkReply->ignoreSslErrors();
    }
    else if (m_ignorePolicy == SslConfig::IgnorePolicy::IgnoreSpecificErrors)
    {
        QList<QSslError> ignorable;
        for (const QSslError &e : errors)
        {
            if (m_resolvedIgnoreErrorTypes.contains(e.error()))
            {
                qWarning() << "[QMultiThreadNetwork] SSL error ignored (specific):" << e.errorString();
                ignorable.append(e);
            }
            else
            {
                qWarning() << "[QMultiThreadNetwork] SSL error NOT ignored:" << e.errorString();
            }
        }
        if (!ignorable.isEmpty() && m_pNetworkReply)
            m_pNetworkReply->ignoreSslErrors(ignorable);
    }
    else
    {
        qWarning() << "[QMultiThreadNetwork] SSL errors (policy=Never) for part" << m_nIndex << m_url.toString() << ":";
        for (const QSslError &e : errors)
            qWarning() << "   " << e.errorString();
    }
}
#endif

void Downloader::onReadyRead()
{
    // N6: Forward data arrival for Layer3 idle timeout via parent NetworkMTDownloadRequest
    emit dataReceived();

    if (m_pNetworkReply && m_pNetworkReply->error() == QNetworkReply::NoError && m_pNetworkReply->isOpen())
    {
        const QByteArray &bytesRev = m_pNetworkReply->readAll();
        if (bytesRev.isEmpty())
            return;

        if (m_mappedFile && m_mappedFile->isOpen())
        {
            // Calculate write position: start position + bytes already written
            qint64 writePosition = m_nStartPoint + m_bytesWritten;

            // Check if it will exceed download range
            qint64 remainingBytes = m_nEndPoint - m_nStartPoint + 1;
            qint64 bytesToWrite = qMin(static_cast<qint64>(bytesRev.size()), remainingBytes - m_bytesWritten);

            if (bytesToWrite > 0)
            {
                qint64 bytesWritten = m_mappedFile->write(writePosition, bytesRev.constData(), bytesToWrite);
                if (bytesWritten > 0)
                {
                    m_bytesWritten += bytesWritten;
                }
                else
                {
                    qCritical() << "[QMultiThreadNetwork] Part" << m_nIndex << "MemoryMappedFile write error:" << m_mappedFile->lastError();
                    m_strError = m_mappedFile->lastError();
                }
            }
            else
            {
                if (!m_bOverflowLogged)
                {
                    m_bOverflowLogged = true;
                    int httpStatus = m_pNetworkReply
                        ? m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                        : -1;
                    qint64 contentLength = m_pNetworkReply
                        ? m_pNetworkReply->header(QNetworkRequest::ContentLengthHeader).toLongLong()
                        : -1;
                    qWarning() << "[QMultiThreadNetwork] Part" << m_nIndex
                               << "Range overflow: expected"
                               << remainingBytes << "bytes, wrote"
                               << m_bytesWritten << "bytes, HTTP status"
                               << httpStatus << "Content-Length"
                               << contentLength;
                }
            }
        }
        else
        {
            qCritical() << "[QMultiThreadNetwork] Part" << m_nIndex << "Memory mapped file is not open";
            m_strError = QString("Memory mapping error: File is not open for memory mapping");
        }
    }
}

void Downloader::onFinished()
{
    try
    {
        if (!m_pNetworkReply)
        {
            m_strError = QString("Network error: Invalid reply");
            emit downloadFinished(m_nIndex, false, m_strError);
            return;
        }

        bool bSuccess = (m_pNetworkReply->error() == QNetworkReply::NoError);
        int statusCode = m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool bHttpProxy = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
        if (bHttpProxy)
        {
            bSuccess = bSuccess && (statusCode >= 200 && statusCode < 300);
        }
        if (!bSuccess)
        {
            // Handle redirection
            if (statusCode == 301 || statusCode == 302)
            {
                const QVariant &redirectionTarget = m_pNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
                const QUrl &redirectUrl = m_url.resolved(redirectionTarget.toUrl());
                if (redirectUrl.isValid() && redirectUrl != m_url && ++m_nRedirectionCount <= m_nMaxRedirectionCount)
                {
                    qDebug() << "[QMultiThreadNetwork] Redirecting from:" << m_url.toString()
                             << "to:" << redirectUrl.toString();

                    // Clean up current resources
                    m_pNetworkReply->deleteLater();
                    m_pNetworkReply = nullptr;

                    start(redirectUrl, m_nStartPoint, m_nEndPoint);
                    return;
                }
            }
            else if (bHttpProxy)
            {
                qDebug() << "[QMultiThreadNetwork] Part" << m_nIndex << "status code: " << statusCode;
            }
            qDebug() << "[QMultiThreadNetwork] Part" << m_nIndex << "download failed!";
        }
        else
        {
            // Memory mapped file syncs automatically, no need to manually flush
            if (m_mappedFile && m_mappedFile->isOpen())
            {
                m_mappedFile->flush();
            }
        }

        // Clean up current resources
        m_pNetworkReply->deleteLater();
        m_pNetworkReply = nullptr;

        emit downloadFinished(m_nIndex, bSuccess, m_strError);
    }
    catch (const std::exception &e)
    {
        m_strError = QString("Download error: Exception occurred in downloader - %1").arg(QString::fromUtf8(e.what()));
        qCritical() << "[QMultiThreadNetwork] Part" << m_nIndex << "Downloader::onFinished() exception:" << m_strError;

        // Ensure signal is emitted to notify failure even in exceptional cases
        emit downloadFinished(m_nIndex, false, m_strError);
        return;
    }
    catch (...)
    {
        m_strError = QString("Download error: Unknown exception occurred in downloader");
#ifdef _WIN32
        DWORD error = GetLastError();
        if (error != 0)
        {
            m_strError = QString("Download error: Unknown downloader exception (System error: %1)").arg(error);
        }
#else
        int error = errno;
        if (error != 0)
        {
            m_strError = QString("Download error: Unknown downloader exception (errno: %1 - %2)").arg(error).arg(QString::fromLatin1(strerror(error)));
        }
#endif
        qCritical() << "[QMultiThreadNetwork] Part" << m_nIndex << "Downloader::onFinished() exception:" << m_strError;

        // Ensure signal is emitted to notify failure even in exceptional cases
        emit downloadFinished(m_nIndex, false, m_strError);
        return;
    }
}

void Downloader::onError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    m_strError = m_pNetworkReply->errorString();
    qDebug() << "[QMultiThreadNetwork] Part" << m_nIndex << "Downloader::onError" << m_strError;
}