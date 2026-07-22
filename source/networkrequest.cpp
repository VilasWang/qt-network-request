#include "networkrequest.h"
#include <QDebug>
#include <QThread>
#include "networkdownloadrequest.h"
#include "networkuploadrequest.h"
#include "networkcommonrequest.h"
#include "networkmtdownloadrequest.h"
#include "networkrequestutility.h"
#include "networkrequestmanager.h"
#include "sharedcookiejar.h"

using namespace QtNetworkRequest;

NetworkRequest::NetworkRequest(QObject *parent)
    : QObject(parent), m_bAbortManual(false), m_pNetworkManager(nullptr), m_pNetworkReply(nullptr), m_nProgress(0), m_nRetryCount(0), m_nRedirectionCount(0)
{
}

NetworkRequest::~NetworkRequest()
{
    if (m_pNetworkReply)
    {
        // Disconnect all signals so the NAM-pooled QNetworkReply (a child of
        // the shared NAM) cannot deliver callbacks to this destroying request.
        m_pNetworkReply->disconnect(this);

        if (m_pNetworkReply->isRunning())
        {
            m_pNetworkReply->abort();
        }
        // Do NOT delete or deleteLater() the reply here.
        // QNetworkReplyImpl's destructor walks back into the shared NAM's
        // private state (connection cache/channel); under stop/cancel timing
        // races this crashes (0xC0000005 at Qt5Network during ~NetworkRequest).
        // deleteLater() is also unsafe: if the NAM is later destroyed by the
        // cleanup QRunnable, the reply (a NAM child) is deleted, and a
        // subsequent deferred-delete event processed on the reused worker
        // thread would dereference a dangling pointer.
        // The reply is a child of the thread-affine NAM and is safely reclaimed
        // by the NAM's own destruction during pool shutdown (cleanup QRunnable
        // runs on the same affine thread). We only abort+disconnect here so it
        // stops doing work and cannot call back into this destroying request.
        m_pNetworkReply = nullptr;
    }
    // Disconnect NAM signals that were connected to this request
    if (m_pNetworkManager)
    {
        m_pNetworkManager->disconnect(this);
        m_pNetworkManager = nullptr;
    }
}

void NetworkRequest::abort()
{
    m_bAbortManual = true;
    m_heartbeatTimer.stop();
    if (m_pNetworkReply)
    {
        // Block signals so any pending queued signal delivery
        // (QMetaCallEvent) is silently dropped rather than routed
        // to this already-destroyed NetworkRequest.
        m_pNetworkReply->blockSignals(true);

        // Disconnect all signals to prevent callbacks after abort
        m_pNetworkReply->disconnect(this);

        if (m_pNetworkReply->isRunning())
        {
            m_pNetworkReply->abort();
        }
        // Use direct delete rather than deleteLater(): when the event loop
        // is about to be quit (cancel path), deferred deletion never runs
        // and the QNetworkReply lives on as a child of the shared NAM,
        // eventually delivering queued signals to a destroyed NetworkRequest.
        delete m_pNetworkReply;
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
        // NAM is owned by the pool — just release our reference
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
    {
        // Use a delegate wrapper so the NAM owns its own SharedCookieJar
        // while the underlying real jar is owned by NetworkRequestManager.
        mgr->setCookieJar(new SharedCookieJar(jar));
    }
}

#ifndef QT_NO_SSL
// Resolve effective SSL config: start from per-request (if provided) else global,
// then fill Inherit fields from the global copy (taken under lock, by value).
SslConfig NetworkRequest::resolveSslConfig(const SslConfig *perRequest)
{
    SslConfig global = NetworkRequestManager::globalSslConfig();  // by-value, thread-safe
    SslConfig base = perRequest ? *perRequest : global;

    if (base.peerVerifyMode == SslConfig::PeerVerifyMode::Inherit)
        base.peerVerifyMode = global.peerVerifyMode;
    if (base.minProtocol == SslConfig::TlsProtocol::Inherit)
        base.minProtocol = global.minProtocol;
    if (base.ignoreSslErrorsPolicy == SslConfig::IgnorePolicy::Inherit)
        base.ignoreSslErrorsPolicy = global.ignoreSslErrorsPolicy;
    if (base.caPolicy == SslConfig::CaPolicy::Inherit)
    {
        base.caPolicy = global.caPolicy;
        base.caCertificates = global.caCertificates;
    }
    return base;
}

static QSsl::SslProtocol toQSslProtocol(SslConfig::TlsProtocol p)
{
    switch (p)
    {
    case SslConfig::TlsProtocol::TlsV1_0: return QSsl::TlsV1_0OrLater;
    case SslConfig::TlsProtocol::TlsV1_1: return QSsl::TlsV1_1OrLater;
    case SslConfig::TlsProtocol::TlsV1_2: return QSsl::TlsV1_2OrLater;
    case SslConfig::TlsProtocol::TlsV1_3:
#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
        return QSsl::TlsV1_3OrLater;
#else
        return QSsl::TlsV1_2OrLater;  // fallback for Qt < 5.12
#endif
    case SslConfig::TlsProtocol::AnyProtocol: return QSsl::AnyProtocol;
    default: return QSsl::TlsV1_2OrLater;
    }
}

void NetworkRequest::applySslConfigToRequest(QNetworkRequest &request, const SslConfig &resolved)
{
    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(
        resolved.peerVerifyMode == SslConfig::PeerVerifyMode::VerifyPeer
            ? QSslSocket::VerifyPeer : QSslSocket::VerifyNone);
    conf.setProtocol(toQSslProtocol(resolved.minProtocol));
    if (resolved.caPolicy == SslConfig::CaPolicy::Custom && !resolved.caCertificates.isEmpty())
        conf.setCaCertificates(resolved.caCertificates);
    request.setSslConfiguration(conf);
}

void NetworkRequest::applySslConfig(QNetworkRequest &request)
{
    // Only HTTPS requests use SSL configuration; for plain HTTP/FTP the
    // QSslConfiguration is ignored by Qt. m_resolvedIgnorePolicy stays at
    // its default (Never) so onSslErrors (never fired for non-HTTPS) is safe.
    if (request.url().scheme().toLower() != "https")
        return;

    const SslConfig *perRequest = (m_upContext && m_upContext->sslConfig)
        ? m_upContext->sslConfig.get() : nullptr;
    SslConfig resolved = resolveSslConfig(perRequest);

    // Cache resolved ignore policy/types for onSslErrors().
    m_resolvedIgnorePolicy = resolved.ignoreSslErrorsPolicy;
    m_resolvedIgnoreErrorTypes = resolved.ignoreErrorTypes;

    applySslConfigToRequest(request, resolved);
}

void NetworkRequest::connectSslErrorHandling(QNetworkReply *reply)
{
    if (!reply)
        return;
    connect(reply, &QNetworkReply::sslErrors, this, &NetworkRequest::onSslErrors);
}

void NetworkRequest::onSslErrors(const QList<QSslError> &errors)
{
    if (m_resolvedIgnorePolicy == SslConfig::IgnorePolicy::Always)
    {
        qWarning() << "[QMultiThreadNetwork] SSL errors IGNORED (policy=Always) for"
                   << m_url.toString() << "- NOT for production use:";
        for (const QSslError &e : errors)
            qWarning() << "   " << e.errorString();
        if (m_pNetworkReply)
            m_pNetworkReply->ignoreSslErrors();
    }
    else if (m_resolvedIgnorePolicy == SslConfig::IgnorePolicy::IgnoreSpecificErrors)
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
        // Never: log and let the handshake fail (SslHandshakeFailedError triggers retry/fail).
        qWarning() << "[QMultiThreadNetwork] SSL errors (policy=Never) for" << m_url.toString() << ":";
        for (const QSslError &e : errors)
            qWarning() << "   " << e.errorString();
    }
}
#endif

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