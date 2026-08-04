#include "networkrequest.h"
#include <QDebug>
#include <QThread>
#include <QNetworkCookie>
#include <QUrlQuery>
#include "networkdownloadrequest.h"
#include "networkuploadrequest.h"
#include "networkcommonrequest.h"
#include "networkmtdownloadrequest.h"
#include "networkrequestutility.h"
#include "networkrequestmanager.h"
#include "networkrequestregistry.h"
#include "sharedcookiejar.h"
#include "qtcompat.h"

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
    m_error = ErrorInfo{};
    m_strError.clear();
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
            setError(ErrorCategory::Timeout, ErrorCode::TimeoutIdle,
                     QStringLiteral("Request idle timeout"),
                     static_cast<int>(QNetworkReply::TimeoutError));
            abort();
            return;
        }
    }

    // Layer2b: Transfer timeout for Qt < 5.15 (reuses heartbeat timer)
    if (QtCompat::isTransferTimedOut(m_transferElapsed, m_upContext->behavior.transferTimeout))
    {
        qWarning() << "[QMultiThreadNetwork] Request transfer timeout (legacy), taskId:" << m_upContext->task.id;
        setError(ErrorCategory::Timeout, ErrorCode::TimeoutTransfer,
                 QStringLiteral("Request transfer timeout"),
                 static_cast<int>(QNetworkReply::TimeoutError));
        if (m_pNetworkReply)
        {
            m_pNetworkReply->abort();
        }
    }
}

void NetworkRequest::resetIdleTimer()
{
    m_idleTimeoutCount = 0;
}

void NetworkRequest::onError(QNetworkReply::NetworkError code)
{
    m_error = makeNetworkError(code, m_pNetworkReply->errorString());
    m_strError = m_error.message;
    qDebug() << "[QMultiThreadNetwork] Error" << QString("[%1]").arg(NetworkRequestUtility::getRequestTypeString(m_upContext->type)) << m_strError;
}

void NetworkRequest::setError(ErrorCategory category, ErrorCode code, const QString& msg, int nativeCode)
{
    m_error.category = category;
    m_error.code = code;
    m_error.nativeCode = nativeCode;
    m_error.message = msg;
    m_strError = msg;
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
        return QtCompat::kHasTlsV1_3 ? QSsl::TlsV1_3OrLater : QSsl::TlsV1_2OrLater;
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

// ============================================================================
// Shared helper methods — consolidated from duplicated subclass code
// ============================================================================

QNetworkRequest NetworkRequest::prepareRequest()
{
    // Acquire thread-affine NAM from pool (once)
    if (nullptr == m_pNetworkManager)
    {
        m_pNetworkManager = NetworkRequestManager::acquireThreadNam();
    }

    // Per-request proxy applies after pool's global proxy
    applyProxyConfig(m_pNetworkManager);

    // Set cookies
    for (QNetworkCookie &cookie : m_upContext->cookies)
    {
        if (m_pNetworkManager->cookieJar())
        {
            m_pNetworkManager->cookieJar()->insertCookie(cookie);
        }
    }

    // --- Build URL: append queryParams and API Key (QueryParam placement) ---
    QUrl url = m_url;
    bool bHasQueryParams = !m_upContext->queryParams.isEmpty();
    bool bHasApiKeyQuery = (m_upContext->authConfig.type == AuthType::ApiKey
                            && m_upContext->authConfig.apiKeyPlacement == ApiKeyPlacement::QueryParam);
    if (bHasQueryParams || bHasApiKeyQuery)
    {
        QUrlQuery query(url);
        if (bHasQueryParams)
        {
            for (auto it = m_upContext->queryParams.cbegin(); it != m_upContext->queryParams.cend(); ++it)
                query.addQueryItem(it.key(), it.value());
        }
        if (bHasApiKeyQuery)
        {
            const AuthConfig &auth = m_upContext->authConfig;
            query.addQueryItem(auth.apiKey, auth.apiValue);
        }
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    QtCompat::setTransferTimeout(request, m_upContext->behavior.transferTimeout);

    // Set custom headers (user-set headers take priority)
    auto iter = m_upContext->headers.cbegin();
    for (; iter != m_upContext->headers.cend(); ++iter)
    {
        request.setRawHeader(iter.key(), iter.value());
    }

    // Apply authentication headers (only if user hasn't explicitly set them)
    applyAuthConfig(request);

    // Auto-detect Content-Type based on BodyType (only if user hasn't set it)
    applyBodyTypeContentType(request);

#ifndef QT_NO_SSL
    applySslConfig(request);
#endif

    return request;
}

std::pair<bool, int> NetworkRequest::evaluateOutcome()
{
    if (!m_pNetworkReply)
        return {false, 0};

    bool bSuccess = (m_pNetworkReply->error() == QNetworkReply::NoError);
    int statusCode = m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    bool bHttpProxy = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
    if (bHttpProxy)
    {
        bSuccess = bSuccess && (statusCode >= 200 && statusCode < 300);
    }

    return {bSuccess, statusCode};
}

bool NetworkRequest::handleFailure()
{
    auto [bSuccess, statusCode] = evaluateOutcome();
    if (bSuccess)
        return false;

    // 1) Try retry first
    if (tryRetry())
        return true;

    // 2) Handle redirection (301/302)
    if (statusCode == 301 || statusCode == 302)
    {
        const QVariant &redirectionTarget = m_pNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        const QUrl &redirectUrl = m_url.resolved(redirectionTarget.toUrl());
        if (redirectUrl.isValid() && m_url != redirectUrl &&
            ++m_nRedirectionCount <= m_upContext->behavior.maxRedirectionCount)
        {
            qDebug() << "[QMultiThreadNetwork] Redirecting from:" << m_url.toString()
                     << "to:" << redirectUrl.toString();
            m_url = redirectUrl;

            // Clean up current resources
            m_pNetworkReply->deleteLater();
            m_pNetworkReply = nullptr;

            // Subclass-specific cleanup
            cleanupForRetry();

            // Restart request
            start();
            return true;
        }
    }
    else
    {
        bool bHttpProxy = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
        if (bHttpProxy)
        {
            qDebug() << "[QMultiThreadNetwork]" << QString("HTTP error: status code %1").arg(statusCode);
        }
    }

    return false; // caller should emit failure
}

void NetworkRequest::applyAuthConfig(QNetworkRequest &request)
{
    const AuthConfig &auth = m_upContext->authConfig;
    if (auth.type == AuthType::None || !auth.isValid())
        return;

    switch (auth.type)
    {
    case AuthType::Basic:
    case AuthType::Bearer:
    {
        QByteArray authVal = auth.authorizationHeaderValue();
        // Case-insensitive: don't overwrite user-set Authorization header
        if (!request.hasRawHeader("Authorization") &&
            !request.hasRawHeader("authorization"))
        {
            request.setRawHeader("Authorization", authVal);
        }
        break;
    }
    case AuthType::ApiKey:
    {
        if (auth.apiKeyPlacement == ApiKeyPlacement::Header)
        {
            QByteArray keyBytes = auth.apiKeyHeaderName();
            if (!request.hasRawHeader(keyBytes))
            {
                request.setRawHeader(keyBytes, auth.apiKeyHeaderValue());
            }
        }
        // QueryParam placement handled in prepareRequest() URL building
        break;
    }
    default:
        break;
    }
}

void NetworkRequest::applyBodyTypeContentType(QNetworkRequest &request)
{
    // Only auto-set Content-Type if user hasn't explicitly set it
    if (request.hasRawHeader("Content-Type") ||
        request.hasRawHeader("content-type"))
        return;

    switch (m_upContext->bodyType)
    {
    case BodyType::Json:
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
        break;
    case BodyType::Xml:
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/xml"));
        break;
    case BodyType::FormUrlEncoded:
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
        break;
    case BodyType::Binary:
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/octet-stream"));
        break;
    case BodyType::None:
    case BodyType::Raw:
    case BodyType::FormData:
        // No automatic Content-Type for these types
        break;
    }
}

QByteArray NetworkRequest::effectiveRequestBody() const
{
    if (m_upContext->bodyType == BodyType::Binary && !m_upContext->binaryBody.isEmpty())
        return m_upContext->binaryBody;
    return m_upContext->body.toUtf8();
}

void NetworkRequest::collectResponse(QMap<QByteArray, QByteArray>& outHeaders, QByteArray& outBody)
{
    if (!m_bAbortManual && m_pNetworkReply && m_pNetworkReply->isOpen())
    {
        outBody = m_pNetworkReply->readAll();
        foreach (const QByteArray &header, m_pNetworkReply->rawHeaderList())
        {
            outHeaders[header] = m_pNetworkReply->rawHeader(header);
        }

        // Extract parsed cookies from Set-Cookie response headers
        if (m_spResult)
        {
            QVariant cookieVar = m_pNetworkReply->header(QNetworkRequest::SetCookieHeader);
            if (cookieVar.isValid())
                m_spResult->cookies = cookieVar.value<QList<QNetworkCookie>>();
        }
    }
}

void NetworkRequest::disposeReply()
{
    if (m_pNetworkReply)
    {
        m_pNetworkReply->deleteLater();
        m_pNetworkReply = nullptr;
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
    // Ensure a failed result always carries an error category.
    if (m_error.category == ErrorCategory::None)
    {
        if (statusCode >= 400)
            m_error = makeHttpError(statusCode, m_strError);
        else
        {
            m_error.category = ErrorCategory::Unknown;
            m_error.code = ErrorCode::Unknown;
        }
    }
    if (m_error.message.isEmpty())
        m_error.message = m_strError;
    m_spResult->error = m_error;
    m_spResult->statusCode = statusCode;
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
    m_spResult->error = ErrorInfo{};
    m_spResult->statusCode = statusCode;
    m_spResult->body = body;
    m_spResult->headers = headers;
    m_spResult->task = m_upContext->task;
    m_spResult->userContext = m_upContext->userContext;
    return m_spResult;
}

std::unique_ptr<NetworkRequest> NetworkRequestFactory::create(std::unique_ptr<RequestContext> context)
{
    if (nullptr == context)
        return nullptr;

    // Delegate to the self-registering factory.
    // Each NetworkRequest subclass registers itself via static initializers,
    // so new types don't require modifications here.
    return NetworkRequestRegistry::instance().create(std::move(context));
}