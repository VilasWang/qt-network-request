#include "networkuploadrequest.h"
#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>
#include <QHttpPart>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include "networkrequestmanager.h"
#include "networkrequestutility.h"
#include "networkrequestevent.h"

using namespace QtNetworkRequest;

NetworkUploadRequest::NetworkUploadRequest(QObject *parent /* = nullptr */)
	: NetworkRequest(parent)
{
	m_timer.setInterval(m_mIntervalMs);
	connect(&m_timer, &QTimer::timeout, this, [this]()
		{
			m_bTimeout = true;
		});
}

NetworkUploadRequest::~NetworkUploadRequest()
{
	m_timer.stop();
	// Improved destructor - ensure proper resource cleanup
	if (m_pFile && m_pFile->isOpen())
	{
		m_pFile->close();
	}
	m_pFile.reset();
}

void NetworkUploadRequest::start()
{
	__super::start();

	const QUrl& url = m_url;
	if (!url.isValid())
	{
		m_strError = QString("Network error: Invalid URL format - %1").arg(url.toString());
		emit response(ToFailedResult());
		return;
	}

	if (nullptr == m_pNetworkManager)
	{
		m_pNetworkManager = new QNetworkAccessManager(this);
		// Set timeout
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		m_pNetworkManager->setTransferTimeout(m_upContext->behavior.transferTimeout);
#endif
	}
	m_pNetworkManager->connectToHost(url.host(), url.port());

	for (QNetworkCookie& cookie : m_upContext->cookies)
	{
		if (m_pNetworkManager->cookieJar())
		{
			m_pNetworkManager->cookieJar()->insertCookie(cookie);
		}
	}

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	// Let Qt automatically handle Content-Length, remove manual setting
	// request.setHeader(QNetworkRequest::ContentLengthHeader, bytes.length());
	request.setRawHeader("Connection", "keep-alive");
	auto iter = m_upContext->headers.cbegin();
	for (; iter != m_upContext->headers.cend(); ++iter)
	{
		request.setRawHeader(iter.key(), iter.value());
	}

	Q_ASSERT(nullptr != m_upContext->uploadConfig);
	QHttpMultiPart* pHttpMultiPart = nullptr;
	bool bFormData = m_upContext->uploadConfig && m_upContext->uploadConfig->useFormData && !m_upContext->uploadConfig->files.isEmpty();
	if (!bFormData)
	{
		m_pFile = NetworkRequestUtility::openFile(m_upContext->uploadConfig->filePath, m_strError);
		if (!m_pFile || !m_pFile->isOpen())
		{
			emit response(ToFailedResult());
			return;
		}
	}
	else
	{
		pHttpMultiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
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
	}

	if (!isFtpProxy(url.scheme())) // http / https
	{
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
		if (m_upContext->uploadConfig->usePutMethod)
		{
			if (bFormData)
			{
				m_pNetworkReply = m_pNetworkManager->put(request, pHttpMultiPart);
				pHttpMultiPart->setParent(m_pNetworkReply);
			}
			else
			{
				m_pNetworkReply = m_pNetworkManager->put(request, m_pFile.get());
			}
		}
		else
		{
			if (bFormData)
			{
				m_pNetworkReply = m_pNetworkManager->post(request, pHttpMultiPart);
				pHttpMultiPart->setParent(m_pNetworkReply);
			}
			else
			{
				m_pNetworkReply = m_pNetworkManager->post(request, m_pFile.get());
			}
		}
	}
	else // ftp
	{
		if (bFormData)
		{
			m_pNetworkReply = m_pNetworkManager->put(request, pHttpMultiPart);
			pHttpMultiPart->setParent(m_pNetworkReply);
		}
		else
		{
			m_pNetworkReply = m_pNetworkManager->put(request, m_pFile.get());
		}
	}

	connect(m_pNetworkReply, SIGNAL(finished()), this, SLOT(onFinished()));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
	connect(m_pNetworkReply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#else
	connect(m_pNetworkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
#endif
	connect(m_pNetworkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
			SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
	if (m_upContext->behavior.showProgress)
	{
		connect(m_pNetworkReply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(onUploadProgress(qint64, qint64)));
	}
	m_timer.start();
}

void NetworkUploadRequest::onFinished()
{
	if (!m_pNetworkReply)
	{
		m_strError = QString("Network error: Invalid reply");
		emit response(ToFailedResult());
		return;
	}

	CloseFile();

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
				qDebug() << "[NetworkUploadRequest] Redirecting from:" << url.toString()
						 << "to:" << redirectUrl.toString();
				m_url = redirectUrl.toString();

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
			qDebug() << "[NetworkUploadRequest]" << QString("HTTP error: status code %1").arg(statusCode);
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
	if (bSuccess)
	{
		qDebug() << "[NetworkDownloadRequest] Upload completed successfully:" << url.toString();
	}
	else
	{
		qDebug() << "[NetworkDownloadRequest] Upload failed:" << m_strError;
	}

	// Clean up current resources
	m_pNetworkReply->deleteLater();
	m_pNetworkReply = nullptr;

    if (bSuccess)
        emit response(ToSuccessResult(body, responseHeaders));
    else
        emit response(ToFailedResult());
}

void NetworkUploadRequest::onUploadProgress(qint64 iSent, qint64 iTotal)
{
	if (m_bAbortManual || !m_bTimeout || iSent <= 0 || iTotal <= 0)
		return;
	m_bTimeout = false;

	int progress = iSent * 100 / iTotal;
	if (m_nProgress < progress)
	{
		m_nProgress = progress;
		NetworkProgressEvent *event = new NetworkProgressEvent;
		event->bDownload = false;
		event->uiId = m_upContext->task.id;
		event->uiBatchId = m_upContext->task.batchId;
		event->iBtyes = iSent;
		event->iTotalBtyes = iTotal;
		QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
	}
}

void NetworkUploadRequest::CloseFile()
{
	if (m_pFile)
	{
		if (m_pFile->isOpen())
		{
			m_pFile->close();
		}

		m_pFile.reset();
	}
}
