#include "networkmtdownloadrequest.h"
#include "memorymappedfile.h"
#include <QtGlobal> // Add Qt version check support
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
    __super::abort();
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

bool NetworkMTDownloadRequest::requestFileSize()
{
    if (!m_url.isValid())
    {
        return false;
    }
    const QUrl& url = m_url;
    m_nFileSize = -1;

    if (nullptr == m_pNetworkManager)
    {
        m_pNetworkManager = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(url);
    request.setRawHeader("Accept-Encoding", "gzip,deflate");

#ifndef QT_NO_SSL
    if (url.scheme().toLower() == "https")
    {
        // Preparation before sending HTTPS request;
        QSslConfiguration conf = request.sslConfiguration();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
        conf.setProtocol(QSsl::TlsV1_2OrLater);
#else
        conf.setProtocol(QSsl::TlsV1_2OrLater);
#endif
        request.setSslConfiguration(conf);
    }
#endif

    m_pNetworkReply = m_pNetworkManager->head(request);
    if (m_pNetworkReply)
    {
        connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        connect(m_pNetworkReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#else
        connect(m_pNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#endif
    }
    return true;
}

void NetworkMTDownloadRequest::start()
{
    __super::start();

    m_nSuccess = 0;
    m_nFailed = 0;
    m_nThreadCount = 1;

    if (!requestFileSize())
    {
        m_strError = "Network error: Invalid URL format";
        emit response(ToFailedResult());
    }
}

void NetworkMTDownloadRequest::startMTDownload()
{
    if (m_bAbortManual)
    {
        return;
    }

    // Start timing
    m_downloadTimer.start();
    if (m_nFileSize <= 0)
    {
        m_strError = "Server error: Content-Length header not provided";
        qDebug() << "[QMultiThreadNetwork]" << m_strError;

        emit response(ToFailedResult());
        return;
    }
    m_strDstFilePath = NetworkRequestUtility::getFilePath(m_upContext.get(), m_strError);
    if (m_strDstFilePath.isEmpty())
    {
        emit response(ToFailedResult());
        return;
    }

    // Generate temporary file path
    m_strTempFilePath = generateTempFilePath(m_strDstFilePath);
    if (m_strTempFilePath.isEmpty())
    {
        m_strError = "Failed to generate temporary file path";
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
        m_strError = QString("Memory mapping error: Failed to create memory mapped file - %1").arg(m_mappedFile->lastError());
        qDebug() << "[QMultiThreadNetwork]" << m_strError;
        emit response(ToFailedResult());
        return;
    }
    clearDownloaders();
    Q_ASSERT(nullptr != m_upContext->downloadConfig);
    m_nThreadCount = m_upContext->downloadConfig->threadCount;
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
                this);

        connect(downloader.get(), SIGNAL(downloadFinished(int, bool, const QString &)),
                this, SLOT(onSubPartFinished(int, bool, const QString &)));
        connect(downloader.get(), SIGNAL(downloadProgress(int, qint64, qint64)),
                this, SLOT(onSubPartDownloadProgress(int, qint64, qint64)));
        if (downloader->start(m_upContext->url, start, end))
        {
            m_mapDownloader[i] = std::move(downloader);
            m_mapBytesReceived.insert(i, 0);
        }
        else
        {
            abort();
            m_strError = QString("Download error: Part %1 failed - %2").arg(i).arg(downloader->errorString());
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
            m_strError = strErr;
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

            // Get response header information (headers from HEAD request)
            QMap<QByteArray, QByteArray> responseHeaders;
            if (m_pNetworkReply)
            {
                foreach(const QByteArray & header, m_pNetworkReply->rawHeaderList())
                {
                    responseHeaders[header] = m_pNetworkReply->rawHeader(header);
                }
            }
            // Close memory mapped file before rename operation
            if (m_mappedFile)
            {
                m_mappedFile->close();
                m_mappedFile.reset();
            }

            // Rename temporary file to final name
            if (!renameTempFileToFinal())
            {
                m_strError = QString("Failed to rename temporary file to final destination: %1").arg(m_strError);
                emit response(ToFailedResult());
                return;
            }

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
			event->iBtyes = totalReceived;
			event->iTotalBtyes = m_bytesTotal;
			QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
		}
	}
}

void NetworkMTDownloadRequest::onFinished()
{
    if (!m_pNetworkReply)
    {
        m_strError = QString("Network error: Invalid reply");
        emit response(ToFailedResult());
        return;
    }

    bool bSuccess = (m_pNetworkReply->error() == QNetworkReply::NoError);
    int statusCode = m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl& url = m_url;
    Q_ASSERT(url.isValid());
    // Check HTTP status code
    bool bHttpProxy = isHttpProxy(url.scheme()) || isHttpsProxy(url.scheme());
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
            if (redirectUrl.isValid() && m_url != redirectUrl &&
                ++m_nRedirectionCount <= m_upContext->behavior.maxRedirectionCount)
            {
                qDebug() << "[QMultiThreadNetwork] url:" << m_url.toString() << "redirectUrl:" << redirectUrl.toString();
                m_url = redirectUrl;

                m_pNetworkReply->deleteLater();
                m_pNetworkReply = nullptr;

                requestFileSize();
                return;
            }
        }
        else if (bHttpProxy)
        {
            qDebug() << "[NetworkMTDownloadRequest]" << QString("HTTP error: status code %1").arg(statusCode);
        }

        m_strError = QString("HTTP error: Failed to retrieve file size - Status code %1").arg(statusCode);
        qDebug() << "[QMultiThreadNetwork]" << m_strError;

        emit response(ToFailedResult());
        return;
    }
    clearProgress();

    for (auto& headerpair : m_pNetworkReply->rawHeaderPairs())
    {
        QString headerLine = QString("%1: %2\n").arg(QString::fromUtf8(headerpair.first)).arg(QString::fromUtf8(headerpair.second));
        qDebug() << headerLine;
    }

    const QVariant &var = m_pNetworkReply->header(QNetworkRequest::ContentLengthHeader);
    m_nFileSize = var.toLongLong();
    m_bytesTotal = m_nFileSize;
    qDebug() << "[QMultiThreadNetwork] File size:" << m_nFileSize;

    m_pNetworkReply->deleteLater();
    m_pNetworkReply = nullptr;

    startMTDownload();
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
Downloader::Downloader(int index, MemoryMappedFile *mappedFile, QNetworkAccessManager *pNetworkManager, bool bShowProgress, quint16 nMaxRedirectionCount, QObject *parent)
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
      m_bytesWritten(0)
{
	m_timer.setInterval(m_mIntervalMs);
	connect(&m_timer, &QTimer::timeout, this, [this]()
		{
			m_bTimeout = true;
		});
}

Downloader::~Downloader()
{
    abort();
}

void Downloader::abort()
{
    m_bAbortManual = true;
    m_timer.stop();
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
    QString range = QString::asprintf("Bytes=%lld-%lld", m_nStartPoint, m_nEndPoint);
    if (range.isEmpty())
    {
        m_strError = QString("Range error: Invalid download range specified");
        return false;
    }
    // According to HTTP protocol, write RANGE header to specify file range request
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("Range", range.toLocal8Bit());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setRawHeader("Accept-Encoding", "gzip,deflate");
    request.setRawHeader("Connection", "keep-alive");

#ifndef QT_NO_SSL
    if (url.scheme().toLower() == "https")
    {
        // Preparation before sending HTTPS request;
        QSslConfiguration conf = request.sslConfiguration();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
        conf.setProtocol(QSsl::TlsV1_2OrLater);
#else
        conf.setProtocol(QSsl::TlsV1_2OrLater);
#endif
        request.setSslConfiguration(conf);
    }
#endif

    qDebug() << "[QMultiThreadNetwork] Part" << m_nIndex << "Range:" << range;

    m_pNetworkReply = m_pNetworkManager->get(request);
    if (m_pNetworkReply)
    {
        connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
        connect(m_pNetworkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        connect(m_pNetworkReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#else
        connect(m_pNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#endif
        
        connect(m_pNetworkReply, &QNetworkReply::downloadProgress, this, [=](qint64 bytesReceived, qint64 bytesTotal)
            {
                if (!m_bAbortManual && m_bTimeout && bytesReceived > 0 && bytesTotal > 0)
                    m_bTimeout = false;
                    emit downloadProgress(m_nIndex, bytesReceived, bytesTotal); 
            });
    }
    m_timer.start();
    return true;
}

void Downloader::onReadyRead()
{
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
                qWarning() << "[QMultiThreadNetwork] Part" << m_nIndex << "Attempted to write beyond download range";
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