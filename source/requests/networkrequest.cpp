#include "networkrequest.h"
#include <QDebug>
#include <QThread>
#include <QNetworkCookie>
#include <QUrlQuery>
#include "networkrequestutils.h"
#include "networkrequestmanager.h"
#include "networkrequestregistry.h"
#include "sharedcookiejar.h"
#include "environment.h"
#include "qtcompat.h"

using namespace QtNetworkRequest;

NetworkRequest::NetworkRequest(QObject *parent)
    : QObject(parent), m_isAbortedManually(false), m_networkManager(nullptr), m_networkReply(nullptr), m_progress(0), m_retryCount(0), m_redirectionCount(0)
{
}

NetworkRequest::~NetworkRequest()
{
    if (m_networkReply)
    {
        // Disconnect all signals so the NAM-pooled QNetworkReply (a child of
        // the shared NAM) cannot deliver callbacks to this destroying request.
        m_networkReply->disconnect(this);

        if (m_networkReply->isRunning())
        {
            m_networkReply->abort();
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
        m_networkReply = nullptr;
    }
    // Disconnect NAM signals that were connected to this request
    if (m_networkManager)
    {
        m_networkManager->disconnect(this);
        m_networkManager = nullptr;
    }
}

void NetworkRequest::abort()
{
    m_isAbortedManually = true;
    m_heartbeatTimer.stop();
    if (m_networkReply)
    {
        // Block signals so any pending queued signal delivery
        // (QMetaCallEvent) is silently dropped rather than routed
        // to this already-destroyed NetworkRequest.
        m_networkReply->blockSignals(true);

        // Disconnect all signals to prevent callbacks after abort
        m_networkReply->disconnect(this);

        if (m_networkReply->isRunning())
        {
            m_networkReply->abort();
        }
        // Use direct delete rather than deleteLater(): when the event loop
        // is about to be quit (cancel path), deferred deletion never runs
        // and the QNetworkReply lives on as a child of the shared NAM,
        // eventually delivering queued signals to a destroyed NetworkRequest.
        delete m_networkReply;
        m_networkReply = nullptr;
    }
}

void NetworkRequest::start()
{
    m_isAbortedManually = false;
    m_progress = 0;
    m_error = ErrorInfo{};
    m_errorMessage.clear();
    m_result = QSharedPointer<ResponseResult>::create();

    // Layer3: Idle/stall timeout
    m_heartbeatTimer.stop();
    m_heartbeatTimer.disconnect();
    int idleMs = m_context ? m_context->behavior.idleTimeoutMs : 0;
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
            qWarning() << "[QMultiThreadNetwork] Request idle timeout, taskId:" << m_context->task.id;
            setError(ErrorCategory::Timeout, ErrorCode::TimeoutIdle,
                     QStringLiteral("Request idle timeout"),
                     static_cast<int>(QNetworkReply::TimeoutError));
            abort();
            return;
        }
    }

    // Layer2b: Transfer timeout for Qt < 5.15 (reuses heartbeat timer)
    if (QtCompat::isTransferTimedOut(m_transferElapsed, m_context->behavior.transferTimeout))
    {
        qWarning() << "[QMultiThreadNetwork] Request transfer timeout (legacy), taskId:" << m_context->task.id;
        setError(ErrorCategory::Timeout, ErrorCode::TimeoutTransfer,
                 QStringLiteral("Request transfer timeout"),
                 static_cast<int>(QNetworkReply::TimeoutError));
        if (m_networkReply)
        {
            m_networkReply->abort();
        }
    }
}

void NetworkRequest::resetIdleTimer()
{
    m_idleTimeoutCount = 0;
}

void NetworkRequest::onError(QNetworkReply::NetworkError code)
{
    m_error = makeNetworkError(code, m_networkReply->errorString());
    m_errorMessage = m_error.message;
    qDebug() << "[QMultiThreadNetwork] Error" << QString("[%1]").arg(NetworkRequestUtils::getRequestTypeString(m_context->type)) << m_errorMessage;
}

void NetworkRequest::setError(ErrorCategory category, ErrorCode code, const QString& msg, int nativeCode)
{
    m_error.category = category;
    m_error.code = code;
    m_error.nativeCode = nativeCode;
    m_error.message = msg;
    m_errorMessage = msg;
}

void NetworkRequest::onAuthenticationRequired(QNetworkReply *r, QAuthenticator *a)
{
    Q_UNUSED(a);
    qDebug() << "[QMultiThreadNetwork] Authentication Required." << r->readAll();
}

void NetworkRequest::applyProxyConfig(QNetworkAccessManager* mgr)
{
    if (m_context && m_context->proxyConfig && m_context->proxyConfig->enabled)
    {
        mgr->setProxy(m_context->proxyConfig->toQNetworkProxy());
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
    if (m_isAbortedManually)
        return false;

    if (!m_context || !m_context->behavior.retryOnFailed)
        return false;
    if (m_retryCount >= m_context->behavior.maxRetryCount)
        return false;

    QNetworkReply::NetworkError code = m_networkReply
        ? m_networkReply->error() : QNetworkReply::UnknownNetworkError;
    if (!isTransientError(code))
        return false;

    m_retryCount++;

    qDebug() << "[QMultiThreadNetwork] Retrying" << m_url.toString()
             << "attempt" << m_retryCount << "/" << m_context->behavior.maxRetryCount;

    if (m_networkReply)
    {
        if (m_networkReply->isRunning())
            m_networkReply->abort();
        m_networkReply->deleteLater();
        m_networkReply = nullptr;
    }
    if (m_networkManager)
    {
        // NAM is owned by the pool — just release our reference
        m_networkManager = nullptr;
    }

    cleanupForRetry();

    int delay = qMin(m_context->behavior.retryDelayMs * (1 << (m_retryCount - 1)), 30000);
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

    const SslConfig *perRequest = (m_context && m_context->sslConfig)
        ? m_context->sslConfig.get() : nullptr;
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
        if (m_networkReply)
            m_networkReply->ignoreSslErrors();
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
        if (!ignorable.isEmpty() && m_networkReply)
            m_networkReply->ignoreSslErrors(ignorable);
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
    if (nullptr == m_networkManager)
    {
        m_networkManager = NetworkRequestManager::acquireThreadNam();
    }

    // Per-request proxy applies after pool's global proxy
    applyProxyConfig(m_networkManager);

    // Set cookies
    for (QNetworkCookie &cookie : m_context->cookies)
    {
        if (m_networkManager->cookieJar())
        {
            m_networkManager->cookieJar()->insertCookie(cookie);
        }
    }

    // --- Build URL: append queryParams and API Key (QueryParam placement) ---
    QUrl url = m_url;
    bool hasQueryParams = !m_context->queryParams.isEmpty();
    bool hasApiKeyQuery = (m_context->authConfig.type == AuthType::ApiKey
                            && m_context->authConfig.apiKeyPlacement == ApiKeyPlacement::QueryParam);
    if (hasQueryParams || hasApiKeyQuery)
    {
        QUrlQuery query(url);
        if (hasQueryParams)
        {
            for (auto it = m_context->queryParams.cbegin(); it != m_context->queryParams.cend(); ++it)
                query.addQueryItem(it.key(), it.value());
        }
        if (hasApiKeyQuery)
        {
            const AuthConfig &auth = m_context->authConfig;
            query.addQueryItem(auth.apiKey, auth.apiValue);
        }
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    QtCompat::setTransferTimeout(request, m_context->behavior.transferTimeout);

    // Set custom headers (user-set headers take priority)
    auto iter = m_context->headers.cbegin();
    for (; iter != m_context->headers.cend(); ++iter)
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
    if (!m_networkReply)
        return {false, 0};

    bool success = (m_networkReply->error() == QNetworkReply::NoError);
    int statusCode = m_networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    bool isProxyScheme = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
    if (isProxyScheme)
    {
        success = success && (statusCode >= 200 && statusCode < 300);
    }

    return {success, statusCode};
}

bool NetworkRequest::handleFailure()
{
    auto [success, statusCode] = evaluateOutcome();
    if (success)
        return false;

    // 1) Try retry first
    if (tryRetry())
        return true;

    // 2) OAuth2 401 auto-refresh (M2): if we got a 401 with OAuth2 auth and
    //    haven't already tried refreshing, attempt one token refresh.
    if (statusCode == 401 &&
        m_context->authConfig.type == AuthType::OAuth2 &&
        !m_isOAuthRefreshed &&
        !m_context->authConfig.oauth2Config.refreshToken.isEmpty())
    {
        qDebug() << "[QMultiThreadNetwork] OAuth2 401 — attempting token refresh";
        m_isOAuthRefreshed = true;

        // Swap to RefreshToken grant and use the stored refresh token.
        // The oauth2Config already has the refreshToken field populated;
        // we just switch the grant type.
        m_context->authConfig.oauth2Config.grant = OAuth2GrantType::RefreshToken;

        // Clean up current resources
        if (m_networkReply)
        {
            m_networkReply->deleteLater();
            m_networkReply = nullptr;
        }
        cleanupForRetry();

        // Restart: start() will see OAuth2 type → fetchOAuth2Token → performSend
        start();
        return true;
    }

    // 3) Handle redirection (301/302)
    if (statusCode == 301 || statusCode == 302)
    {
        const QVariant &redirectionTarget = m_networkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        const QUrl &redirectUrl = m_url.resolved(redirectionTarget.toUrl());
        if (redirectUrl.isValid() && m_url != redirectUrl &&
            ++m_redirectionCount <= m_context->behavior.maxRedirectionCount)
        {
            qDebug() << "[QMultiThreadNetwork] Redirecting from:" << m_url.toString()
                     << "to:" << redirectUrl.toString();
            m_url = redirectUrl;

            // Clean up current resources
            m_networkReply->deleteLater();
            m_networkReply = nullptr;

            // Subclass-specific cleanup
            cleanupForRetry();

            // Restart request
            start();
            return true;
        }
    }
    else
    {
        bool isProxyScheme = isHttpProxy(m_url.scheme()) || isHttpsProxy(m_url.scheme());
        if (isProxyScheme)
        {
            qDebug() << "[QMultiThreadNetwork]" << QString("HTTP error: status code %1").arg(statusCode);
        }
    }

    return false; // caller should emit failure
}

void NetworkRequest::applyAuthConfig(QNetworkRequest &request)
{
    const AuthConfig &auth = m_context->authConfig;
    if (auth.type == AuthType::None || !auth.isValid())
        return;

    switch (auth.type)
    {
    case AuthType::Basic:
    case AuthType::Bearer:
    case AuthType::OAuth2:
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

    switch (m_context->bodyType)
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
    if (m_context->bodyType == BodyType::Binary && !m_context->binaryBody.isEmpty())
        return m_context->binaryBody;
    return m_context->body.toUtf8();
}

void NetworkRequest::collectResponse(QMap<QByteArray, QByteArray>& outHeaders, QByteArray& outBody)
{
    if (!m_isAbortedManually && m_networkReply && m_networkReply->isOpen())
    {
        outBody = m_networkReply->readAll();
        foreach (const QByteArray &header, m_networkReply->rawHeaderList())
        {
            outHeaders[header] = m_networkReply->rawHeader(header);
        }

        // Extract parsed cookies from Set-Cookie response headers
        if (m_result)
        {
            QVariant cookieVar = m_networkReply->header(QNetworkRequest::SetCookieHeader);
            if (cookieVar.isValid())
                m_result->cookies = cookieVar.value<QList<QNetworkCookie>>();
        }
    }
}

void NetworkRequest::disposeReply()
{
    if (m_networkReply)
    {
        m_networkReply->deleteLater();
        m_networkReply = nullptr;
    }
}

void NetworkRequest::setRequestContext(std::unique_ptr<RequestContext> context)
{
    if (context)
    {
        m_context = std::move(context);
        m_isOAuthRefreshed = false;   // reset the 401-refresh guard for each new request
        // (M1) Substitute {{var}} placeholders across url/headers/body/query/auth
        // before the QUrl is finalized, so the resolved value is used downstream.
        if (!m_context->environment.isEmpty())
            applyEnvironment(*m_context, m_context->environment);
        m_url = QUrl(m_context->url);
    }
}

QSharedPointer<QtNetworkRequest::ResponseResult> NetworkRequest::ToFailedResult(int statusCode, const QByteArray& body, const QMap<QByteArray, QByteArray>& headers)
{
    if (!m_result)
    {
        m_result = QSharedPointer<ResponseResult>::create();
    }
    // Ensure a failed result always carries an error category.
    if (m_error.category == ErrorCategory::None)
    {
        if (statusCode >= 400)
            m_error = makeHttpError(statusCode, m_errorMessage);
        else
        {
            m_error.category = ErrorCategory::Unknown;
            m_error.code = ErrorCode::Unknown;
        }
    }
    if (m_error.message.isEmpty())
        m_error.message = m_errorMessage;
    m_result->error = m_error;
    m_result->statusCode = statusCode;
    m_result->body = body;
    m_result->headers = headers;
    m_result->task = m_context->task;
    m_result->userContext = m_context->userContext;
    return m_result;
}

QSharedPointer<QtNetworkRequest::ResponseResult> NetworkRequest::ToSuccessResult(const QByteArray& body, const QMap<QByteArray, QByteArray>& headers, int statusCode)
{
    if (!m_result)
    {
        m_result = QSharedPointer<ResponseResult>::create();
    }
    m_result->error = ErrorInfo{};
    m_result->statusCode = statusCode;
    m_result->body = body;
    m_result->headers = headers;
    m_result->task = m_context->task;
    m_result->userContext = m_context->userContext;
    return m_result;
}