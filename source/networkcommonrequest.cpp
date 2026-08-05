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

#include "networkrequestutility.h"
#include "networkrequestmanager.h"
#include "oauth2tokencache.h"
#include "qtcompat.h"
#include "networkrequestregistry.h"
#include "QThread"
#include "QHttpMultiPart"

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
    m_nBytesReceived = 0;
    m_nBytesSent = 0;

    // Estimate bytes sent from request body
    if (m_upContext->type == RequestType::Post || m_upContext->type == RequestType::Put)
    {
        if (m_upContext->uploadConfig && m_upContext->uploadConfig->useFormData)
            m_nBytesSent = m_upContext->body.toUtf8().size(); // estimate
        else if (m_upContext->uploadConfig && !m_upContext->uploadConfig->filePath.isEmpty())
        {
            QFileInfo fi(m_upContext->uploadConfig->filePath);
            m_nBytesSent = fi.size();
        }
        else
            m_nBytesSent = effectiveRequestBody().size();
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
        if (m_upContext->type == RequestType::Post || m_upContext->type == RequestType::Delete || m_upContext->type == RequestType::Head)
        {
            const QString &strType = NetworkRequestUtility::getRequestTypeString(m_upContext->type);
            setError(ErrorCategory::Protocol, ErrorCode::UnsupportedProtocol,
                     QString("Protocol error: Unsupported FTP request type '%1' for URL: %2").arg(strType).arg(url.url()));
            qDebug() << "[QMultiThreadNetwork]" << m_strError;

            emit response(ToFailedResult());
            return;
        }
    }

    // Validate authentication configuration
    if (m_upContext->authConfig.type != AuthType::None && !m_upContext->authConfig.isValid())
    {
        setError(ErrorCategory::Configuration, ErrorCode::AuthInvalid,
                 QString("Authentication error: Invalid credentials for auth type %1")
                     .arg(static_cast<int>(m_upContext->authConfig.type)));
        emit response(ToFailedResult());
        return;
    }

    // OAuth2: fetch a fresh token before sending. Non-OAuth2 requests proceed directly.
    if (m_upContext->authConfig.type == AuthType::OAuth2)
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
    if (!m_upContext->headers.contains("User-Agent") && !m_upContext->headers.contains("user-agent"))
    {
        request.setRawHeader("User-Agent", "QtNetworkRequest/2.0");
    }

    if (m_upContext->type == RequestType::Get)
    {
        m_pNetworkReply = m_pNetworkManager->get(request);
    }
    else if (m_upContext->type == RequestType::Patch)
    {
        const QByteArray &bytes = effectiveRequestBody();
        m_pNetworkReply = m_pNetworkManager->sendCustomRequest(request, "PATCH", bytes);
    }
    else if (m_upContext->type == RequestType::Options)
    {
        m_pNetworkReply = m_pNetworkManager->sendCustomRequest(request, "OPTIONS");
    }
    else if (m_upContext->type == RequestType::Post)
    {
        bool bFormData = m_upContext->uploadConfig && m_upContext->uploadConfig->useFormData && !m_upContext->uploadConfig->files.isEmpty();
        if (!bFormData)
        {
            // Only default to application/x-www-form-urlencoded when bodyType is None (backward compat)
            if (m_upContext->bodyType == BodyType::None &&
                !request.hasRawHeader("Content-Type") && !request.hasRawHeader("content-type"))
            {
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            }

            const QByteArray &bytes = effectiveRequestBody();
            // Let Qt automatically handle Content-Length, remove manual setting
            // request.setHeader(QNetworkRequest::ContentLengthHeader, bytes.length());

            m_pNetworkReply = m_pNetworkManager->post(request, bytes);
        }
        else
        {
            Q_ASSERT(nullptr != m_upContext->uploadConfig);
            QHttpMultiPart *pHttpMultiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            auto& files = m_upContext->uploadConfig->files;
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
				file->setParent(pHttpMultiPart); // Will be set when multiPart is created

				// Add file field
				QHttpPart filePart;
				filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeType));
                QString disposition = QString("form-data; name=\"file\"; filename=\"%1\"").arg(fileInfo.fileName());
				filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));
				filePart.setBodyDevice(file);
                pHttpMultiPart->append(filePart);
            }
			auto& kvPairs = m_upContext->uploadConfig->kvPairs;
            for (auto iter = kvPairs.begin(); iter != kvPairs.end(); ++iter)
            {
				// Handle plain text
				// Add text field
				QHttpPart textPart;
				QString disposition = QString("form-data; name=\"%1\"")
					.arg(iter.key());
				textPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(disposition));
				textPart.setBody(iter.value().toUtf8());
                pHttpMultiPart->append(textPart);
            }
            request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=" + pHttpMultiPart->boundary());
            
            m_pNetworkReply = m_pNetworkManager->post(request, pHttpMultiPart);
            pHttpMultiPart->setParent(m_pNetworkReply);
        }
    }
            else if (m_upContext->type == RequestType::Put)
            {
                Q_ASSERT(nullptr != m_upContext->uploadConfig);
                if (!m_upContext->uploadConfig->filePath.isEmpty() && QFile::exists(m_upContext->uploadConfig->filePath))
                {
                    QFile* file = new QFile(m_upContext->uploadConfig->filePath);
                    if (!file->open(QIODevice::ReadOnly)) {
                        setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed,
                                 "Failed to open file for PUT: " + file->errorString());
                        delete file;
                        emit response(ToFailedResult());
                        return;
                    }
                    m_pNetworkReply = m_pNetworkManager->put(request, file);
                    file->setParent(m_pNetworkReply); // The reply will take ownership of the file device
                }
                else
                {
                    const QByteArray &bytes = effectiveRequestBody();
                    m_pNetworkReply = m_pNetworkManager->put(request, bytes);
                }
            }    else if (m_upContext->type == RequestType::Delete)
    {
        m_pNetworkReply = m_pNetworkManager->deleteResource(request);
    }
    else if (m_upContext->type == RequestType::Head)
    {
        m_pNetworkReply = m_pNetworkManager->head(request);
    }

    connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
    connect(m_pNetworkReply, &QNetworkReply::readyRead, this, [this]() { resetIdleTimer(); });
    QtCompat::connectErrorSignal(m_pNetworkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
    connect(m_pNetworkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
            SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
#ifndef QT_NO_SSL
    connectSslErrorHandling(m_pNetworkReply);
#endif

    // Layer2b: transfer timeout via elapsed timer (no-op on Qt >= 5.15)
    QtCompat::startTransferTimer(m_transferElapsed);
}

void NetworkCommonRequest::fetchOAuth2Token(std::function<void()> onReady)
{
    const auto &oa = m_upContext->authConfig.oauth2Config;

    // Build cache key: tokenUrl|clientId|grant|scopes
    const QString cacheKey = OAuth2TokenCache::makeKey(
        oa.tokenUrl, oa.clientId,
        QString::number(static_cast<int>(oa.grant)), oa.scopes);

    // Check cache first (synchronous fast path)
    auto &cache = OAuth2TokenCache::instance();
    OAuth2TokenEntry cachedEntry;
    if (cache.find(cacheKey, cachedEntry) && cachedEntry.isValid())
    {
        m_upContext->authConfig.token = cachedEntry.accessToken;
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
        m_upContext->authConfig.token = accessToken;

        onReady();
    });
}

void NetworkCommonRequest::onFinished()
{
    if (!m_pNetworkReply)
    {
        setError(ErrorCategory::Network, ErrorCode::InvalidReply,
                 QString("Network error: Invalid reply"));
        emit response(ToFailedResult());
        return;
    }

    auto [bSuccess, statusCode] = evaluateOutcome();
    if (!bSuccess && handleFailure())
        return;

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    QByteArray body;
    if (bSuccess)
        collectResponse(responseHeaders, body);

    disposeReply();

    m_nBytesReceived = body.size();
    if (m_spResult)
    {
        m_spResult->performance.bytesReceived = m_nBytesReceived;
        m_spResult->performance.bytesSent = m_nBytesSent;
    }

    if (bSuccess)
        emit response(ToSuccessResult(body, responseHeaders, statusCode));
    else
        emit response(ToFailedResult(statusCode));
}
