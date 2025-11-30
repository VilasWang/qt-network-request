#include "networkcommonrequest.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>

#include "networkrequestutility.h"
#include <QtGlobal> // Add header file for Qt version checking
#include "QThread"
#include "QHttpMultiPart"

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

    const QUrl &url = m_url;
    if (!url.isValid())
    {
        m_strError = QString("Network error: Invalid URL format - %1").arg(url.toString());
        emit response(ToFailedResult());
        return;
    }

    if (isFtpProxy(url.scheme()))
    {
        if (m_upContext->type == RequestType::Post || m_upContext->type == RequestType::Delete || m_upContext->type == RequestType::Head)
        {
            const QString &strType = NetworkRequestUtility::getRequestTypeString(m_upContext->type);
            m_strError = QString("Protocol error: Unsupported FTP request type '%1' for URL: %2").arg(strType).arg(url.url());
            qDebug() << "[QMultiThreadNetwork]" << m_strError;

            emit response(ToFailedResult());
            return;
        }
    }

    if (nullptr == m_pNetworkManager)
    {
        m_pNetworkManager = new QNetworkAccessManager(this);
// Set timeout
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        m_pNetworkManager->setTransferTimeout(m_upContext->behavior.transferTimeout);
#endif
    }
    for (QNetworkCookie &cookie : m_upContext->cookies)
    {
        if (m_pNetworkManager->cookieJar())
        {
            m_pNetworkManager->cookieJar()->insertCookie(cookie);
        }
    }

    QNetworkRequest request(url);

    // Set default User-Agent if not provided
    if (!m_upContext->headers.contains("User-Agent") && !m_upContext->headers.contains("user-agent"))
    {
        request.setRawHeader("User-Agent", "QtNetworkRequest/2.0");
    }

    auto iter = m_upContext->headers.cbegin();
    for (; iter != m_upContext->headers.cend(); ++iter)
    {
        request.setRawHeader(iter.key(), iter.value());
    }

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

    if (m_upContext->type == RequestType::Get)
    {
        m_pNetworkReply = m_pNetworkManager->get(request);
    }
    else if (m_upContext->type == RequestType::Post)
    {
        bool bFormData = m_upContext->uploadConfig && m_upContext->uploadConfig->useFormData && !m_upContext->uploadConfig->files.isEmpty();
        if (!bFormData)
        {
            if (!request.hasRawHeader("Content-Type"))
            {
                // Use application/x-www-form-urlencoded by default
                request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            }

            const QByteArray &bytes = m_upContext->body.toUtf8();
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
                        m_strError = "Failed to open file for PUT: " + file->errorString();
                        delete file;
                        emit response(ToFailedResult());
                        return;
                    }
                    m_pNetworkReply = m_pNetworkManager->put(request, file);
                    file->setParent(m_pNetworkReply); // The reply will take ownership of the file device
                }
                else
                {
                    const QByteArray &bytes = m_upContext->body.toUtf8();
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(m_pNetworkReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#else
    connect(m_pNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#endif
    connect(m_pNetworkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
            SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
}

void NetworkCommonRequest::onFinished()
{
    if (!m_pNetworkReply)
    {
        m_strError = QString("Network error: Invalid reply");
        emit response(ToFailedResult());
        return;
    }

    bool bSuccess = (m_pNetworkReply->error() == QNetworkReply::NoError);
    int statusCode = m_pNetworkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl &url = m_url;
    Q_ASSERT(url.isValid());

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
            const QUrl &redirectUrl = url.resolved(redirectionTarget.toUrl());
            if (redirectUrl.isValid() && url != redirectUrl && ++m_nRedirectionCount <= m_upContext->behavior.maxRedirectionCount)
            {
                qDebug() << "[NetworkCommonRequest] Redirecting from:" << url.toString()
                         << "to:" << redirectUrl.toString();
                m_url = redirectUrl;

                // Clean up current resources
                m_pNetworkReply->deleteLater();
                m_pNetworkReply = nullptr;

                // Restart request
                start();
                return;
            }
        }
        else if (bHttpProxy)
        {
            qDebug() << "[NetworkCommonRequest]" << QString("HTTP error: status code %1").arg(statusCode);
        }
    }

    // Get response header information
    QMap<QByteArray, QByteArray> responseHeaders;
    QByteArray body;
    if (!m_bAbortManual && m_pNetworkReply->isOpen()) // Not ended by calling abort()
    {
        if (bSuccess)
        {
            body = m_pNetworkReply->readAll();
            foreach(const QByteArray & header, m_pNetworkReply->rawHeaderList())
            {
                responseHeaders[header] = m_pNetworkReply->rawHeader(header);
            }
        }
    }

    // Clean up current resources
    m_pNetworkReply->deleteLater();
    m_pNetworkReply = nullptr;

    if (bSuccess)
        emit response(ToSuccessResult(body, responseHeaders));
    else
        emit response(ToFailedResult());
}
