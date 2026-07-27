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
#include "qtcompat.h"
#include "networkrequestregistry.h"

// Self-registration: single-threaded download
namespace {
	[[maybe_unused]] static const int _regDownload = []() -> int {
		using namespace QtNetworkRequest;
		NetworkRequestRegistry::instance().registerCreator(
			RequestType::Download, 5,
			[](RequestContext*) -> NetworkRequest* {
				return new NetworkDownloadRequest();
			});
		return 0;
	}();
}

using namespace QtNetworkRequest;

NetworkDownloadRequest::NetworkDownloadRequest(QObject *parent)
    : NetworkRequest(parent), m_pFile(nullptr)
{
	m_throttle = std::make_unique<ProgressThrottle>(250, this);
}

NetworkDownloadRequest::~NetworkDownloadRequest()
{
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

    QNetworkRequest request = prepareRequest();
    // NOTE: Do NOT set "Accept-Encoding" manually. Qt transparently negotiates
    // and decompresses gzip/deflate only when it adds the header itself; a manual
    // header disables auto-decompression and would persist raw compressed bytes.
    // NOTE: Do NOT set "Connection" manually either. It is a hop-by-hop header that
    // Qt manages via its connection pool (keep-alive is the HTTP/1.1 default, and
    // the header is forbidden under HTTP/2 which Qt may negotiate).
    request.setRawHeader("User-Agent", "QtNetworkRequest/2.0");

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
    QtCompat::connectErrorSignal(m_pNetworkReply, this, SLOT(onError(QNetworkReply::NetworkError)));

    if (m_upContext->behavior.showProgress)
    {
        connect(m_pNetworkReply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(onDownloadProgress(qint64, qint64)));
    }

#ifndef QT_NO_SSL
    connectSslErrorHandling(m_pNetworkReply);
#endif

    // Layer2b: transfer timeout via elapsed timer (no-op on Qt >= 5.15)
    QtCompat::startTransferTimer(m_transferElapsed);

    // Start the progress throttle so onDownloadProgress() fires at most once per interval.
    m_throttle->start();
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

    auto [bSuccess, statusCode] = evaluateOutcome();
    if (!bSuccess && handleFailure())
        return;

    // Clean up file (delete on failure, keep on success)
    CloseFile(!bSuccess);

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    if (bSuccess)
    {
        if (!m_bAbortManual && m_pNetworkReply->isOpen())
        {
            foreach(const QByteArray & header, m_pNetworkReply->rawHeaderList())
            {
                responseHeaders[header] = m_pNetworkReply->rawHeader(header);
            }
        }
        qDebug() << "[NetworkDownloadRequest] Download completed successfully:" << m_url.toString();
    }
    else
    {
        qDebug() << "[NetworkDownloadRequest] Download failed:" << m_strError;
    }

    disposeReply();

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

    if (m_bAbortManual)
        return;

    m_throttle->report(iReceived, iTotal, [this](qint64 bytes, qint64 total) {
        int progress = static_cast<int>(bytes * 100 / total);
        if (m_nProgress < progress)
        {
            m_nProgress = progress;
            NetworkProgressEvent *event = new NetworkProgressEvent;
            event->uiId = m_upContext->task.id;
            event->uiBatchId = m_upContext->task.batchId;
            event->iBytes = bytes;
            event->iTotalBytes = total;
            QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
        }
    });
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
