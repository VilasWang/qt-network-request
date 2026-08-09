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
#include "networkrequestutils.h"
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
    : NetworkRequest(parent), m_file(nullptr)
{
	m_throttle = std::make_unique<ProgressThrottle>(250, this);
}

NetworkDownloadRequest::~NetworkDownloadRequest()
{
    // Improved destructor - ensure proper resource cleanup
    if (m_file && m_file->isOpen())
    {
        m_file->close();
    }
    m_file.reset();
}

void NetworkDownloadRequest::start()
{
    NetworkRequest::start();
    m_bytesReceived = 0;
    m_bytesWritten = 0;

    const QUrl &url = m_url;
    if (!url.isValid())
    {
        setError(ErrorCategory::Configuration, ErrorCode::InvalidUrl,
                 QString("Network error: Invalid URL format - %1").arg(url.toString()));
        qDebug() << "[NetworkDownloadRequest]" << m_errorMessage;
        emit response(ToFailedResult());
        return;
    }

    // Improved file creation - use smart pointers for exception safety
    try
    {
        m_file = std::move(NetworkRequestUtils::createAndOpenFile(m_context.get(), m_errorMessage));
        if (!m_file || !m_file->isOpen())
        {
            setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, m_errorMessage);
            qDebug() << "[NetworkDownloadRequest] Failed to create/open file:" << m_errorMessage;
            emit response(ToFailedResult());
            return;
        }
    }
    catch (const std::exception &e)
    {
        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed,
                 QString("File system error: Exception occurred while creating file - %1").arg(e.what()));
        qDebug() << "[NetworkDownloadRequest]" << m_errorMessage;
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

    m_networkReply = m_networkManager->get(request);
    if (!m_networkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply,
                 "Network operation failed: Unable to create network request");
        qDebug() << "[NetworkDownloadRequest]" << m_errorMessage;
        emit response(ToFailedResult());
        return;
    }

    // Connect signals
    connect(m_networkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_networkReply, SIGNAL(finished()), this, SLOT(onFinished()));
    QtCompat::connectErrorSignal(m_networkReply, this, SLOT(onError(QNetworkReply::NetworkError)));

    if (m_context->behavior.showProgress)
    {
        connect(m_networkReply, SIGNAL(downloadProgress(qint64, qint64)),
                this, SLOT(onDownloadProgress(qint64, qint64)));
    }

#ifndef QT_NO_SSL
    connectSslErrorHandling(m_networkReply);
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

    if (!m_networkReply || m_networkReply->error() != QNetworkReply::NoError || !m_networkReply->isOpen())
    {
        return;
    }

    if (!m_file || !m_file->isOpen())
    {
        qDebug() << "[NetworkDownloadRequest] File not open for writing";
        return;
    }

    const QByteArray bytesReceived = m_networkReply->readAll();
    if (!bytesReceived.isEmpty())
    {
        qint64 written = m_file->write(bytesReceived);
        if (written == -1)
        {
            qDebug() << "[NetworkDownloadRequest] Write error:" << m_file->errorString();
            setError(ErrorCategory::FileIo, ErrorCode::FileWriteFailed,
                     QString("File operation failed: Write operation failed - %1").arg(m_file->errorString()));
        }
        else
        {
            m_bytesWritten += written;
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
    if (!m_networkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply, "Network error: Invalid reply");
        emit response(ToFailedResult());
        return;
    }

    auto [success, statusCode] = evaluateOutcome();
    if (!success && handleFailure())
        return;

    // Clean up file (delete on failure, keep on success)
    CloseFile(!success);

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    if (success)
    {
        if (!m_isAbortedManually && m_networkReply->isOpen())
        {
            foreach(const QByteArray & header, m_networkReply->rawHeaderList())
            {
                responseHeaders[header] = m_networkReply->rawHeader(header);
            }
        }
        qDebug() << "[NetworkDownloadRequest] Download completed successfully:" << m_url.toString();
    }
    else
    {
        qDebug() << "[NetworkDownloadRequest] Download failed:" << m_errorMessage;
    }

    disposeReply();

    m_bytesReceived = m_bytesWritten;
    if (m_result)
    {
        m_result->performance.bytesReceived = m_bytesReceived;
    }

    if (success)
        emit response(ToSuccessResult({}, responseHeaders, statusCode));
    else
        emit response(ToFailedResult(statusCode));
}

void NetworkDownloadRequest::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    // Reset idle timeout on data arrival
    if (bytesReceived > 0)
        resetIdleTimer();

    if (m_isAbortedManually)
        return;

    m_throttle->report(bytesReceived, bytesTotal, [this](qint64 bytes, qint64 total) {
        int progress = static_cast<int>(bytes * 100 / total);
        if (m_progress < progress)
        {
            m_progress = progress;
            NetworkProgressEvent *event = new NetworkProgressEvent;
            event->requestId = m_context->task.id;
            event->batchId = m_context->task.batchId;
            event->transferredBytes = bytes;
            event->totalBytes = total;
            QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
        }
    });
}

void NetworkDownloadRequest::CloseFile(bool shouldRemove)
{
    if (m_file)
    {
        if (m_file->isOpen())
        {
            m_file->close();
        }

        if (shouldRemove && m_file->exists())
        {
            m_file->remove();
        }
        m_file.reset();
    }
}
