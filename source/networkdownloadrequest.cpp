#include "networkdownloadrequest.h"
#include <memory>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QCoreApplication>
#include "networkrequestmanager.h"
#include "networkrequestutility.h"
#include "networkrequestevent.h"

using namespace QtNetworkRequest;

NetworkDownloadRequest::NetworkDownloadRequest(QObject *parent)
    : NetworkRequest(parent), m_pFile(nullptr)
{
	m_timer.setInterval(m_mIntervalMs);
	connect(&m_timer, &QTimer::timeout, this, [this]()
		{
			m_readyToEmitProgress = true;
		});
}

NetworkDownloadRequest::~NetworkDownloadRequest()
{
    m_timer.stop();
    // Improved destructor - ensure proper resource cleanup
    if (m_pFile && m_pFile->isOpen())
    {
        m_pFile->close();
    }
    m_pFile.reset();
}

void NetworkDownloadRequest::start()
{
    NetworkRequest::start();
    m_nBytesReceived = 0;
    m_nBytesWritten = 0;

    const QUrl &url = m_url;
    if (!url.isValid())
    {
        setError(ErrorCategory::Configuration, ErrorCode::InvalidUrl,
                 QString("Network error: Invalid URL format - %1").arg(url.toString()));
        qDebug() << "[NetworkDownloadRequest]" << m_strError;
        emit response(ToFailedResult());
        return;
    }

    // Improved file creation - use smart pointers for exception safety
    try
    {
        m_pFile = std::move(NetworkRequestUtility::createAndOpenFile(m_upContext.get(), m_strError));
        if (!m_pFile || !m_pFile->isOpen())
        {
            setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, m_strError);
            qDebug() << "[NetworkDownloadRequest] Failed to create/open file:" << m_strError;
            emit response(ToFailedResult());
            return;
        }
    }
    catch (const std::exception &e)
    {
        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed,
                 QString("File system error: Exception occurred while creating file - %1").arg(e.what()));
        qDebug() << "[NetworkDownloadRequest]" << m_strError;
        emit response(ToFailedResult());
        return;
    }

    // Get thread-affine NAM from pool
    if (nullptr == m_pNetworkManager)
    {
        m_pNetworkManager = NetworkRequestManager::acquireThreadNam();
    }
    // Per-request proxy applies after pool's global proxy
    applyProxyConfig(m_pNetworkManager);

    // Set cookies
    for (const QNetworkCookie &cookie : m_upContext->cookies)
    {
        if (m_pNetworkManager->cookieJar())
        {
            m_pNetworkManager->cookieJar()->insertCookie(cookie);
        }
    }

    QNetworkRequest request(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    request.setTransferTimeout(m_upContext->behavior.transferTimeout);
#endif
    // NOTE: Do NOT set "Accept-Encoding" manually. Qt transparently negotiates
    // and decompresses gzip/deflate only when it adds the header itself; a manual
    // header disables auto-decompression and would persist raw compressed bytes.
    // NOTE: Do NOT set "Connection" manually either. It is a hop-by-hop header that
    // Qt manages via its connection pool (keep-alive is the HTTP/1.1 default, and
    // the header is forbidden under HTTP/2 which Qt may negotiate).
    request.setRawHeader("User-Agent", "QtNetworkRequest/2.0");

    // Set custom headers
    auto iter = m_upContext->headers.cbegin();
    for (; iter != m_upContext->headers.cend(); ++iter)
    {
        request.setRawHeader(iter.key(), iter.value());
    }

#ifndef QT_NO_SSL
    applySslConfig(request);
#endif

    m_pNetworkReply = m_pNetworkManager->get(request);
    if (!m_pNetworkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply,
                 "Network operation failed: Unable to create network request");
        qDebug() << "[NetworkDownloadRequest]" << m_strError;
        emit response(ToFailedResult());
        return;
    }

    // Connect signals
    connect(m_pNetworkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(m_pNetworkReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#else
    connect(m_pNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#endif

    if (m_upContext->behavior.showProgress)
    {
        connect(m_pNetworkReply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(onDownloadProgress(qint64, qint64)));
    }

#ifndef QT_NO_SSL
    connectSslErrorHandling(m_pNetworkReply);
#endif

#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    // Layer2b: Qt < 5.15 transfer timeout via elapsed timer
    if (m_upContext->behavior.transferTimeout > 0)
    {
        m_transferElapsed.start();
    }
#endif

    // Start the progress-throttle timer so onDownloadProgress() is allowed to
    // emit at most once per interval. Mirrors NetworkUploadRequest and
    // NetworkMTDownloadRequest; without it a single-thread download would never
    // set m_readyToEmitProgress and downloadProgress() would never fire.
    m_timer.start();
}

void NetworkDownloadRequest::onReadyRead()
{
    // Reset idle timeout on data arrival
    resetIdleTimer();

    if (!m_pNetworkReply || m_pNetworkReply->error() != QNetworkReply::NoError || !m_pNetworkReply->isOpen())
    {
        return;
    }

    if (!m_pFile || !m_pFile->isOpen())
    {
        qDebug() << "[NetworkDownloadRequest] File not open for writing";
        return;
    }

    const QByteArray bytesReceived = m_pNetworkReply->readAll();
    if (!bytesReceived.isEmpty())
    {
        qint64 written = m_pFile->write(bytesReceived);
        if (written == -1)
        {
            qDebug() << "[NetworkDownloadRequest] Write error:" << m_pFile->errorString();
            setError(ErrorCategory::FileIo, ErrorCode::FileWriteFailed,
                     QString("File operation failed: Write operation failed - %1").arg(m_pFile->errorString()));
        }
        else
        {
            m_nBytesWritten += written;
            if (written != bytesReceived.size())
            {
                qDebug() << "[NetworkDownloadRequest] Partial write: expected" << bytesReceived.size()
                         << "wrote" << written;
            }
        }
    }
}

void NetworkDownloadRequest::onFinished()
{
    if (!m_pNetworkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply, "Network error: Invalid reply");
        emit response(ToFailedResult());
        return;
    }

    bool bSuccess = (m_pNetworkReply->error() == QNetworkReply::NoError);
    int statusCode = m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl &url = m_url;
    Q_ASSERT(url.isValid());

    // Check HTTP status code
    bool bHttpProxy = isHttpProxy(url.scheme()) || isHttpsProxy(url.scheme());
    if (bHttpProxy)
    {
        bSuccess = bSuccess && (statusCode >= 200 && statusCode < 300);
    }
    if (!bSuccess)
    {
        if (tryRetry())
            return;
        // Handle redirection
        if (statusCode == 301 || statusCode == 302)
        {
            const QVariant &redirectionTarget = m_pNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            const QUrl &redirectUrl = url.resolved(redirectionTarget.toUrl());
            if (redirectUrl.isValid() && url != redirectUrl && ++m_nRedirectionCount <= m_upContext->behavior.maxRedirectionCount)
            {
                qDebug() << "[NetworkDownloadRequest] Redirecting from:" << url.toString()
                         << "to:" << redirectUrl.toString();
                m_url = redirectUrl.toString();

                // Clean up current resources
                m_pNetworkReply->deleteLater();
                m_pNetworkReply = nullptr;

                CloseFile(true);

                // Restart request
                start();
                return;
            }
        }
        else if (bHttpProxy)
        {
            qDebug() << "[NetworkDownloadRequest]" << QString("HTTP error: status code %1").arg(statusCode);
        }
    }

    // Clean up file
    CloseFile(!bSuccess);

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    if (!m_bAbortManual && m_pNetworkReply->isOpen()) // Not ended by calling abort()
    {
        if (bSuccess)
        {
            foreach(const QByteArray & header, m_pNetworkReply->rawHeaderList())
            {
                responseHeaders[header] = m_pNetworkReply->rawHeader(header);
            }
        }
    }

    if (bSuccess)
    {
        qDebug() << "[NetworkDownloadRequest] Download completed successfully:" << url.toString();
    }
    else
    {
        qDebug() << "[NetworkDownloadRequest] Download failed:" << m_strError;
    }

    // Clean up current resources
    m_pNetworkReply->deleteLater();
    m_pNetworkReply = nullptr;

    m_nBytesReceived = m_nBytesWritten;
    if (m_spResult)
    {
        m_spResult->performance.bytesReceived = m_nBytesReceived;
    }

    if (bSuccess)
        emit response(ToSuccessResult({}, responseHeaders, statusCode));
    else
        emit response(ToFailedResult(statusCode));
}

void NetworkDownloadRequest::onDownloadProgress(qint64 iReceived, qint64 iTotal)
{
    // Reset idle timeout on data arrival
    if (iReceived > 0)
        resetIdleTimer();

    if (m_bAbortManual || !m_readyToEmitProgress || iReceived <= 0 || iTotal <= 0)
        return;

    int progress = static_cast<int>(iReceived * 100 / iTotal);
    if (m_nProgress < progress)
    {
        m_readyToEmitProgress = false;

        m_nProgress = progress;
        NetworkProgressEvent *event = new NetworkProgressEvent;
        event->uiId = m_upContext->task.id;
        event->uiBatchId = m_upContext->task.batchId;
        event->iBytes = iReceived;
        event->iTotalBytes = iTotal;
        QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
    }
}

void NetworkDownloadRequest::CloseFile(bool bRemove)
{
    if (m_pFile)
    {
        if (m_pFile->isOpen())
        {
            m_pFile->close();
        }

        if (bRemove && m_pFile->exists())
        {
            m_pFile->remove();
        }
        m_pFile.reset();
    }
}
