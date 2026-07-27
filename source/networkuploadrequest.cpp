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
#include "qtcompat.h"
#include "networkrequestregistry.h"

// Self-registration: upload
namespace {
	[[maybe_unused]] static const int _regUpload = []() -> int {
		using namespace QtNetworkRequest;
		NetworkRequestRegistry::instance().registerCreator(
			RequestType::Upload, 5,
			[](RequestContext*) -> NetworkRequest* {
				return new NetworkUploadRequest();
			});
		return 0;
	}();
}

using namespace QtNetworkRequest;

NetworkUploadRequest::NetworkUploadRequest(QObject *parent /* = nullptr */)
	: NetworkRequest(parent)
{
	m_throttle = std::make_unique<ProgressThrottle>(250, this);
}

NetworkUploadRequest::~NetworkUploadRequest()
{
	// Improved destructor - ensure proper resource cleanup
	if (m_pFile && m_pFile->isOpen())
	{
		m_pFile->close();
	}
	m_pFile.reset();
}

void NetworkUploadRequest::start()
{
	NetworkRequest::start();
	m_nBytesSent = 0;
	m_nLastSentBytes = 0;

	// Estimate bytes sent
	if (m_upContext->uploadConfig)
	{
		if (!m_upContext->uploadConfig->filePath.isEmpty())
		{
			QFileInfo fi(m_upContext->uploadConfig->filePath);
			m_nBytesSent = fi.size();
		}
		else
			m_nBytesSent = m_upContext->uploadConfig->data.size();
	}

	const QUrl& url = m_url;
	if (!url.isValid())
	{
		setError(ErrorCategory::Configuration, ErrorCode::InvalidUrl,
		         QString("Network error: Invalid URL format - %1").arg(url.toString()));
		emit response(ToFailedResult());
		return;
	}

	if (nullptr == m_pNetworkManager)
	{
		m_pNetworkManager = NetworkRequestManager::acquireThreadNam();
	}
	// Per-request proxy applies after pool's global proxy
	applyProxyConfig(m_pNetworkManager);
	m_pNetworkManager->connectToHost(url.host(), url.port());

	for (QNetworkCookie& cookie : m_upContext->cookies)
	{
		if (m_pNetworkManager->cookieJar())
		{
			m_pNetworkManager->cookieJar()->insertCookie(cookie);
		}
	}

	QNetworkRequest request(url);
	QtCompat::setTransferTimeout(request, m_upContext->behavior.transferTimeout);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	// Let Qt automatically handle Content-Length, remove manual setting
	// request.setHeader(QNetworkRequest::ContentLengthHeader, bytes.length());
	// NOTE: Do NOT set "Connection" manually. It is a hop-by-hop header that Qt
	// manages via its connection pool (keep-alive is the HTTP/1.1 default, and the
	// header is forbidden under HTTP/2 which Qt may negotiate).
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
			setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, m_strError);
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
		applySslConfig(request);
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
	QtCompat::connectErrorSignal(m_pNetworkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
	connect(m_pNetworkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
			SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
#ifndef QT_NO_SSL
	connectSslErrorHandling(m_pNetworkReply);
#endif
	if (m_upContext->behavior.showProgress)
	{
		connect(m_pNetworkReply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(onUploadProgress(qint64, qint64)));
	}

	// Layer2b: transfer timeout via elapsed timer (no-op on Qt >= 5.15)
	QtCompat::startTransferTimer(m_transferElapsed);
	m_throttle->start();
}

void NetworkUploadRequest::onFinished()
{
	if (!m_pNetworkReply)
	{
		setError(ErrorCategory::Network, ErrorCode::InvalidReply, QString("Network error: Invalid reply"));
		emit response(ToFailedResult());
		return;
	}

	CloseFile();

	auto [bSuccess, statusCode] = evaluateOutcome();
	if (!bSuccess && handleFailure())
		return;

	// Get response header information
	QMap<QByteArray, QByteArray> responseHeaders;
	QByteArray body;
	if (bSuccess)
		collectResponse(responseHeaders, body);

	if (bSuccess)
		qDebug() << "[NetworkDownloadRequest] Upload completed successfully:" << m_url.toString();
	else
		qDebug() << "[NetworkDownloadRequest] Upload failed:" << m_strError;

	disposeReply();

    m_nBytesSent = qMax(m_nBytesSent, m_nLastSentBytes);
    if (m_spResult)
    {
        m_spResult->performance.bytesSent = m_nBytesSent;
        m_spResult->performance.bytesReceived = body.size();
    }

	if (bSuccess)
		emit response(ToSuccessResult(body, responseHeaders, statusCode));
	else
		emit response(ToFailedResult(statusCode));
}

void NetworkUploadRequest::onUploadProgress(qint64 iSent, qint64 iTotal)
{
	m_nLastSentBytes = iSent;

    // Reset idle timeout on data sent
    if (iSent > 0)
        resetIdleTimer();

	if (m_bAbortManual)
		return;

	m_throttle->report(iSent, iTotal, [this](qint64 bytes, qint64 total) {
		int progress = bytes * 100 / total;
		if (m_nProgress < progress)
		{
			m_nProgress = progress;
			NetworkProgressEvent *event = new NetworkProgressEvent;
			event->bDownload = false;
			event->uiId = m_upContext->task.id;
			event->uiBatchId = m_upContext->task.batchId;
			event->iBytes = bytes;
			event->iTotalBytes = total;
			QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
		}
	});
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
