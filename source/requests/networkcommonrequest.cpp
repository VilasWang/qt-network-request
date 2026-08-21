#include "networkcommonrequest.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

#include "networkrequestutils.h"
#include "networkrequestmanager.h"
#include "oauth2tokencache.h"
#include "qtcompat.h"
#include "networkrequestregistry.h"
#include <QThread>
#include <QHttpMultiPart>

// Self-registration: register for all common request types
namespace {
	[[maybe_unused]] static const int _regCommonRequests = []() -> int {
		using namespace QtNetworkRequest;
		for (auto type : {RequestType::Get, RequestType::Post, RequestType::Put,
						  RequestType::Delete, RequestType::Head,
						  RequestType::Patch, RequestType::Options})
		{
			NetworkRequestRegistry::instance().registerCreator(
				type, 5,
				[](RequestContext*) -> NetworkRequest* {
					return new NetworkCommonRequest();
				});
		}
		return 0;
	}();
}

using namespace QtNetworkRequest;

NetworkCommonRequest::NetworkCommonRequest(QObject *parent /* = nullptr */)
    : NetworkRequest(parent)
{
}

NetworkCommonRequest::~NetworkCommonRequest()
{
}

void NetworkCommonRequest::start()
{
    NetworkRequest::start();
    m_bytesReceived = 0;
    m_bytesSent = 0;

    // Estimate bytes sent from request body
    if (m_context->type == RequestType::Post || m_context->type == RequestType::Put)
    {
        if (m_context->uploadConfig && m_context->uploadConfig->useFormData)
            m_bytesSent = m_context->body.toUtf8().size(); // estimate
        else if (m_context->uploadConfig && !m_context->uploadConfig->filePath.isEmpty())
        {
            QFileInfo fi(m_context->uploadConfig->filePath);
            m_bytesSent = fi.size();
        }
        else
            m_bytesSent = effectiveRequestBody().size();
    }

    const QUrl &url = m_url;
    if (!url.isValid())
    {
        setError(ErrorCategory::Configuration, ErrorCode::InvalidUrl,
                 QString("Network error: Invalid URL format - %1").arg(url.toString()));
        emit response(ToFailedResult());
        return;
    }

    if (isFtpProxy(url.scheme()))
    {
        if (m_context->type == RequestType::Post || m_context->type == RequestType::Delete || m_context->type == RequestType::Head)
        {
            const QString &requestTypeString = NetworkRequestUtils::getRequestTypeString(m_context->type);
            setError(ErrorCategory::Protocol, ErrorCode::UnsupportedProtocol,
                     QString("Protocol error: Unsupported FTP request type '%1' for URL: %2").arg(requestTypeString).arg(url.url()));
            qDebug() << "[QMultiThreadNetwork]" << m_errorMessage;

            emit response(ToFailedResult());
            return;
        }
    }

    // Validate authentication configuration
    if (m_context->authConfig.type != AuthType::None && !m_context->authConfig.isValid())
    {
        setError(ErrorCategory::Configuration, ErrorCode::AuthInvalid,
                 QString("Authentication error: Invalid credentials for auth type %1")
                     .arg(static_cast<int>(m_context->authConfig.type)));
        emit response(ToFailedResult());
        return;
    }

    // OAuth2: fetch a fresh token before sending. Non-OAuth2 requests proceed directly.
    if (m_context->authConfig.type == AuthType::OAuth2)
    {
        fetchOAuth2Token([this]() { performSend(); });
        return;
    }

    performSend();
}

void NetworkCommonRequest::performSend()
{
    QNetworkRequest request = prepareRequest();

    // Set default User-Agent if not provided (prepareRequest sets custom headers,
    // but we add User-Agent as a default if missing)
    if (!m_context->headers.contains("User-Agent") && !m_context->headers.contains("user-agent"))
    {
        request.setRawHeader("User-Agent", "QtNetworkRequest/2.0");
    }

    if (m_context->type == RequestType::Get)
    {
        m_networkReply = m_networkManager->get(request);
    }
    else if (m_context->type == RequestType::Patch)
    {
        const QByteArray &bytes = effectiveRequestBody();
        m_networkReply = QtCompat::sendCustomRequest(m_networkManager, request, "PATCH", bytes);
    }
    else if (m_context->type == RequestType::Options)
    {
        m_networkReply = QtCompat::sendCustomRequest(m_networkManager, request, "OPTIONS");
    }
    else if (m_context->type == RequestType::Post)
    {
        bool isFormData = m_context->uploadConfig && m_context->uploadConfig->useFormData && !m_context->uploadConfig->files.isEmpty();
        if (!isFormData)
        {
            // Only default to application/x-www-form-urlencoded when bodyType is None (backward compat)
            if (m_context->bodyType == BodyType::None &&
                !request.hasRawHeader("Content-Type") && !request.hasRawHeader("content-type"))
            {
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            }

            const QByteArray &bytes = effectiveRequestBody();
            // Let Qt automatically handle Content-Length, remove manual setting
            // request.setHeader(QNetworkRequest::ContentLengthHeader, bytes.length());

            m_networkReply = m_networkManager->post(request, bytes);
        }
        else
        {
            Q_ASSERT(nullptr != m_context->uploadConfig);
            QHttpMultiPart *httpMultiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            auto& files = m_context->uploadConfig->files;
            for (auto& filePath : files)
            {
				QFileInfo fileInfo(filePath);
                if (!fileInfo.exists())
                {
                    continue;
                }
				QString mimeType = QMimeDatabase().mimeTypeForFile(fileInfo).name();

				// Read file content
				QFile* file = new QFile(filePath);
				if (!file || !file->open(QIODevice::ReadOnly))
				{
                    if (file)
                        delete file;
					continue;
				}
				file->setParent(httpMultiPart); // Will be set when multiPart is created

				// Add file field
				QHttpPart filePart;
				filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeType));
                QString disposition = QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileInfo.fileName());
				filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));
				filePart.setBodyDevice(file);
                httpMultiPart->append(filePart);
            }
			auto& kvPairs = m_context->uploadConfig->kvPairs;
            for (auto iter = kvPairs.begin(); iter != kvPairs.end(); ++iter)
            {
				// Handle plain text
				// Add text field
				QHttpPart textPart;
				QString disposition = QString("form-data; name=\"%1\"")
					.arg(iter.key());
				textPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));
				textPart.setBody(iter.value().toUtf8());
                httpMultiPart->append(textPart);
            }
            request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=" + httpMultiPart->boundary());
            
            m_networkReply = m_networkManager->post(request, httpMultiPart);
            httpMultiPart->setParent(m_networkReply);
        }
    }
            else if (m_context->type == RequestType::Put)
            {
                Q_ASSERT(nullptr != m_context->uploadConfig);
                if (!m_context->uploadConfig->filePath.isEmpty() && QFile::exists(m_context->uploadConfig->filePath))
                {
                    QFile* file = new QFile(m_context->uploadConfig->filePath);
                    if (!file->open(QIODevice::ReadOnly)) {
                        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed,
                                 "Failed to open file for PUT: " + file->errorString());
                        delete file;
                        emit response(ToFailedResult());
                        return;
                    }
                    m_networkReply = m_networkManager->put(request, file);
                    file->setParent(m_networkReply); // The reply will take ownership of the file device
                }
                else
                {
                    const QByteArray &bytes = effectiveRequestBody();
                    m_networkReply = m_networkManager->put(request, bytes);
                }
            }    else if (m_context->type == RequestType::Delete)
    {
        m_networkReply = m_networkManager->deleteResource(request);
    }
    else if (m_context->type == RequestType::Head)
    {
        m_networkReply = m_networkManager->head(request);
    }

    connect(m_networkReply, SIGNAL(finished()), this, SLOT(onFinished()));
    connect(m_networkReply, &QNetworkReply::readyRead, this, [this]() { resetIdleTimer(); });
    QtCompat::connectErrorSignal(m_networkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
    connect(m_networkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
            SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
#ifndef QT_NO_SSL
    connectSslErrorHandling(m_networkReply);
#endif

    // Layer2b: transfer timeout via elapsed timer (no-op on Qt >= 5.15)
    QtCompat::startTransferTimer(m_transferElapsed);
}

void NetworkCommonRequest::fetchOAuth2Token(std::function<void()> onReady)
{
    const auto &oa = m_context->authConfig.oauth2Config;

    // Build cache key: tokenUrl|clientId|grant|scopes
    const QString cacheKey = OAuth2TokenCache::makeKey(
        oa.tokenUrl, oa.clientId,
        QString::number(static_cast<int>(oa.grant)), oa.scopes);

    // Check cache first (synchronous fast path)
    auto &cache = OAuth2TokenCache::instance();
    OAuth2TokenEntry cachedEntry;
    if (cache.find(cacheKey, cachedEntry) && cachedEntry.isValid())
    {
        m_context->authConfig.token = cachedEntry.accessToken;
        onReady();
        return;
    }

    // Build token request body
    QUrlQuery params;
    params.addQueryItem("client_id", oa.clientId);
    params.addQueryItem("client_secret", oa.clientSecret);
    if (!oa.scopes.isEmpty())
        params.addQueryItem("scope", oa.scopes);

    switch (oa.grant)
    {
    case OAuth2GrantType::ClientCredentials:
        params.addQueryItem("grant_type", "client_credentials");
        break;
    case OAuth2GrantType::Password:
        params.addQueryItem("grant_type", "password");
        params.addQueryItem("username", oa.username);
        params.addQueryItem("password", oa.password);
        break;
    case OAuth2GrantType::RefreshToken:
        params.addQueryItem("grant_type", "refresh_token");
        params.addQueryItem("refresh_token", oa.refreshToken);
        break;
    }

    QNetworkRequest tokenReq(QUrl(oa.tokenUrl));
    tokenReq.setHeader(QNetworkRequest::ContentTypeHeader,
                       "application/x-www-form-urlencoded");

    // Acquire a thread-affine NAM for the token request
    QNetworkAccessManager *nam = NetworkRequestManager::acquireThreadNam();
    const QByteArray postBody = params.toString().toUtf8();
    QNetworkReply *reply = nam->post(tokenReq, postBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, cacheKey, onReady]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            setError(ErrorCategory::Network, ErrorCode::TemporaryNetworkFailure,
                     QString("OAuth2 token request failed: %1").arg(reply->errorString()));
            emit response(ToFailedResult());
            return;
        }

        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject())
        {
            setError(ErrorCategory::Configuration, ErrorCode::AuthInvalid,
                     "OAuth2 token response is not valid JSON");
            emit response(ToFailedResult());
            return;
        }

        const QJsonObject obj = doc.object();
        const QString accessToken  = obj["access_token"].toString();
        const QString refreshToken = obj["refresh_token"].toString();
        const int expiresIn        = obj["expires_in"].toInt(3600);

        if (accessToken.isEmpty())
        {
            setError(ErrorCategory::Configuration, ErrorCode::AuthInvalid,
                     "OAuth2 token response missing access_token");
            emit response(ToFailedResult());
            return;
        }

        // Cache the result
        OAuth2TokenEntry entry;
        entry.accessToken  = accessToken;
        entry.refreshToken = refreshToken;
        entry.expiresAt    = QDateTime::currentDateTime().addSecs(expiresIn);
        OAuth2TokenCache::instance().store(cacheKey, entry);

        // Set the token on the auth config so applyAuthConfig can use it
        m_context->authConfig.token = accessToken;

        onReady();
    });
}

void NetworkCommonRequest::onFinished()
{
    if (!m_networkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply,
                 QString("Network error: Invalid reply"));
        emit response(ToFailedResult());
        return;
    }

    auto [success, statusCode] = evaluateOutcome();
    if (!success && handleFailure())
        return;

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    QByteArray body;
    if (success)
        collectResponse(responseHeaders, body);

    disposeReply();

    m_bytesReceived = body.size();
    if (m_result)
    {
        m_result->performance.bytesReceived = m_bytesReceived;
        m_result->performance.bytesSent = m_bytesSent;
    }

    if (success)
        emit response(ToSuccessResult(body, responseHeaders, statusCode));
    else
        emit response(ToFailedResult(statusCode));
}
