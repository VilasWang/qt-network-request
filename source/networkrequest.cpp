#include "networkrequest.h"
#include <QDebug>
#include <QThread>
#include "networkdownloadrequest.h"
#include "networkuploadrequest.h"
#include "networkcommonrequest.h"
#include "networkmtdownloadrequest.h"
#include "networkrequestutility.h"
#include "networkrequestmanager.h"

using namespace QtNetworkRequest;

NetworkRequest::NetworkRequest(QObject *parent)
    : QObject(parent), m_bAbortManual(false), m_pNetworkManager(nullptr), m_pNetworkReply(nullptr), m_nProgress(0), m_nRetryCount(0), m_nRedirectionCount(0)
{
}

NetworkRequest::~NetworkRequest()
{
    if (m_pNetworkReply)
    {
        if (m_pNetworkReply->isRunning())
        {
            m_pNetworkReply->abort();
        }
        // deleteLater() is ineffective inside a destructor — the object's
        // event loop is about to exit and deferred-delete events will never
        // be processed. Use direct delete instead.
        delete m_pNetworkReply;
        m_pNetworkReply = nullptr;
    }
    if (m_pNetworkManager)
    {
        delete m_pNetworkManager;
        m_pNetworkManager = nullptr;
    }
}

void NetworkRequest::abort()
{
    m_bAbortManual = true;
    m_heartbeatTimer.stop();
    if (m_pNetworkReply)
    {
        if (m_pNetworkReply->isRunning())
        {
            m_pNetworkReply->abort();
        }
        m_pNetworkReply->deleteLater();
        m_pNetworkReply = nullptr;
    }
}

void NetworkRequest::start()
{
    m_bAbortManual = false;
    m_nProgress = 0;
    m_spResult = QSharedPointer<ResponseResult>::create();

    // Layer3: Idle/stall timeout
    m_heartbeatTimer.stop();
    m_heartbeatTimer.disconnect();
    int idleMs = m_upContext ? m_upContext->behavior.idleTimeoutMs : 0;
    if (idleMs > 0)
    {
        m_idleThreshold = qMax(1, idleMs / 250);
        m_idleTimeoutCount = 0;
        connect(&m_heartbeatTimer, &QTimer::timeout, this, &NetworkRequest::onHeartbeat);
        m_heartbeatTimer.start(250);
    }
}

void NetworkRequest::onHeartbeat()
{
    // Layer3: Idle/stall detection
    if (m_idleThreshold > 0)
    {
        m_idleTimeoutCount++;
        if (m_idleTimeoutCount >= m_idleThreshold)
        {
            qWarning() << "[QMultiThreadNetwork] Request idle timeout, taskId:" << m_upContext->task.id;
            abort();
            return;
        }
    }

    // Layer2b: Transfer timeout for Qt < 5.15 (reuses heartbeat timer)
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    if (m_transferElapsed.isValid() &&
        m_upContext &&
        m_transferElapsed.elapsed() > m_upContext->behavior.transferTimeout)
    {
        qWarning() << "[QMultiThreadNetwork] Request transfer timeout (legacy), taskId:" << m_upContext->task.id;
        if (m_pNetworkReply)
        {
            m_pNetworkReply->abort();
        }
    }
#endif
}

void NetworkRequest::resetIdleTimer()
{
    m_idleTimeoutCount = 0;
}

void NetworkRequest::onError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);

    m_strError = m_pNetworkReply->errorString();
    qDebug() << "[QMultiThreadNetwork] Error" << QString("[%1]").arg(NetworkRequestUtility::getRequestTypeString(m_upContext->type)) << m_strError;
}

void NetworkRequest::onAuthenticationRequired(QNetworkReply *r, QAuthenticator *a)
{
    Q_UNUSED(a);
    qDebug() << "[QMultiThreadNetwork] Authentication Required." << r->readAll();
}

void NetworkRequest::applyProxyConfig(QNetworkAccessManager* mgr)
{
    if (m_upContext && m_upContext->proxyConfig && m_upContext->proxyConfig->enabled)
    {
        mgr->setProxy(m_upContext->proxyConfig->toQNetworkProxy());
        return;
    }
    const ProxyConfig& global = NetworkRequestManager::globalProxy();
    if (global.enabled)
    {
        mgr->setProxy(global.toQNetworkProxy());
    }
}

bool NetworkRequest::tryRetry()
{
    // N3 fix: Do not retry if abort was triggered by timeout/cancellation
    if (m_bAbortManual)
        return false;

    if (!m_upContext || !m_upContext->behavior.retryOnFailed)
        return false;
    if (m_nRetryCount >= m_upContext->behavior.maxRetryCount)
        return false;

    QNetworkReply::NetworkError code = m_pNetworkReply
        ? m_pNetworkReply->error() : QNetworkReply::UnknownNetworkError;
    if (!isTransientError(code))
        return false;

    m_nRetryCount++;

    qDebug() << "[QMultiThreadNetwork] Retrying" << m_url.toString()
             << "attempt" << m_nRetryCount << "/" << m_upContext->behavior.maxRetryCount;

    if (m_pNetworkReply)
    {
        if (m_pNetworkReply->isRunning())
            m_pNetworkReply->abort();
        m_pNetworkReply->deleteLater();
        m_pNetworkReply = nullptr;
    }
    if (m_pNetworkManager)
    {
        m_pNetworkManager->deleteLater();
        m_pNetworkManager = nullptr;
    }

    cleanupForRetry();

    int delay = qMin(m_upContext->behavior.retryDelayMs * (1 << (m_nRetryCount - 1)), 30000);
    QTimer::singleShot(delay, this, &NetworkRequest::start);

    return true;
}

void NetworkRequest::cleanupForRetry()
{
}

void NetworkRequest::applyCookieJar(QNetworkAccessManager* mgr)
{
    QNetworkCookieJar *jar = NetworkRequestManager::cookieJar();
    if (jar)
        mgr->setCookieJar(jar);
}

bool NetworkRequest::isTransientError(QNetworkReply::NetworkError err)
{
    switch (err)
    {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TooManyRedirectsError:
        return true;
    default:
        return false;
    }
}

void NetworkRequest::setRequestContext(std::unique_ptr<RequestContext> context)
{
    if (context)
    {
        m_upContext = std::move(context);
        m_url = QUrl(m_upContext->url);
    }
}

QSharedPointer<QtNetworkRequest::ResponseResult> NetworkRequest::ToFailedResult(int statusCode, const QByteArray& body, const QMap<QByteArray, QByteArray>& headers)
{
    if (!m_spResult)
    {
        m_spResult = QSharedPointer<ResponseResult>::create();
    }
    m_spResult->success = false;
    m_spResult->statusCode = statusCode;
    m_spResult->errorMessage = m_strError;
    m_spResult->body = body;
    m_spResult->headers = headers;
    m_spResult->task = m_upContext->task;
    m_spResult->userContext = m_upContext->userContext;
    return m_spResult;
}

QSharedPointer<QtNetworkRequest::ResponseResult> NetworkRequest::ToSuccessResult(const QByteArray& body, const QMap<QByteArray, QByteArray>& headers, int statusCode)
{
    if (!m_spResult)
    {
        m_spResult = QSharedPointer<ResponseResult>::create();
    }
    m_spResult->success = true;
    m_spResult->statusCode = statusCode;
    m_spResult->errorMessage.clear();
    m_spResult->body = body;
    m_spResult->headers = headers;
    m_spResult->task = m_upContext->task;
    m_spResult->userContext = m_upContext->userContext;
    return m_spResult;
}

std::unique_ptr<NetworkRequest> NetworkRequestFactory::create(std::unique_ptr<RequestContext> context)
{
    std::unique_ptr<NetworkRequest> pRequest;
    if (nullptr == context)
    {
        return pRequest;
    }
    switch (context->type)
    {
    case RequestType::Download:
    {
	if (context->downloadConfig->threadCount > 1 || context->downloadConfig->threadCount == 0)
		{
            pRequest = std::make_unique<NetworkMTDownloadRequest>();
		}
        else
		{
			pRequest = std::make_unique<NetworkDownloadRequest>();
        }
    }
    break;
    case RequestType::MTDownload:
    {
        pRequest = std::make_unique<NetworkMTDownloadRequest>();
    }
    break;
    case RequestType::Upload:
    {
        pRequest = std::make_unique<NetworkUploadRequest>();
    }
    break;
    case RequestType::Post:
    case RequestType::Get:
    case RequestType::Put:
    case RequestType::Delete:
    case RequestType::Head:
    case RequestType::Patch:
    case RequestType::Options:
    {
        pRequest = std::make_unique<NetworkCommonRequest>();
    }
    break;
    /*New type add to here*/
    default:
        break;
    }
    if (pRequest)
    {
        pRequest->setRequestContext(std::move(context));
    }
    return pRequest;
}