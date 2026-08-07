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
#include "networkrequestutils.h"
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
	if (m_file && m_file->isOpen())
	{
		m_file->close();
	}
	m_file.reset();
}

void NetworkUploadRequest::start()
{
	NetworkRequest::start();
	m_bytesSent = 0;
	m_lastSentBytes = 0;

	// Estimate bytes sent
	if (m_context->uploadConfig)
	{
		if (!m_context->uploadConfig->filePath.isEmpty())
		{
			QFileInfo fi(m_context->uploadConfig->filePath);
			m_bytesSent = fi.size();
		}
		else
			m_bytesSent = m_context->uploadConfig->data.size();
	}

	const QUrl& url = m_url;
	if (!url.isValid())
	{
		setError(ErrorCategory::Configuration, ErrorCode::InvalidUrl,
		         QString("Network error: Invalid URL format - %1").arg(url.toString()));
		emit response(ToFailedResult());
		return;
	}

	if (nullptr == m_networkManager)
	{
		m_networkManager = NetworkRequestManager::acquireThreadNam();
	}
	// Per-request proxy applies after pool's global proxy
	applyProxyConfig(m_networkManager);
	m_networkManager->connectToHost(url.host(), url.port());

	for (QNetworkCookie& cookie : m_context->cookies)
	{
		if (m_networkManager->cookieJar())
		{
			m_networkManager->cookieJar()->insertCookie(cookie);
		}
	}

	QNetworkRequest request(url);
	QtCompat::setTransferTimeout(request, m_context->behavior.transferTimeout);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	// Let Qt automatically handle Content-Length, remove manual setting
	// request.setHeader(QNetworkRequest::ContentLengthHeader, bytes.length());
	// NOTE: Do NOT set "Connection" manually. It is a hop-by-hop header that Qt
	// manages via its connection pool (keep-alive is the HTTP/1.1 default, and the
	// header is forbidden under HTTP/2 which Qt may negotiate).
	auto iter = m_context->headers.cbegin();
	for (; iter != m_context->headers.cend(); ++iter)
	{
		request.setRawHeader(iter.key(), iter.value());
	}

	Q_ASSERT(nullptr != m_context->uploadConfig);
	QHttpMultiPart* httpMultiPart = nullptr;
	bool bFormData = m_context->uploadConfig && m_context->uploadConfig->useFormData && !m_context->uploadConfig->files.isEmpty();
	if (!bFormData)
	{
		m_file = NetworkRequestUtils::openFile(m_context->uploadConfig->filePath, m_errorMessage);
		if (!m_file || !m_file->isOpen())
		{
			setError(ErrorCategory::FileIo, ErrorCode::FileOpenFailed, m_errorMessage);
			emit response(ToFailedResult());
			return;
		}
	}
	else
	{
		httpMultiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
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
	}

	if (!isFtpProxy(url.scheme())) // http / https
	{
#ifndef QT_NO_SSL
		applySslConfig(request);
#endif
		if (m_context->uploadConfig->usePutMethod)
		{
			if (bFormData)
			{
				m_networkReply = m_networkManager->put(request, httpMultiPart);
				httpMultiPart->setParent(m_networkReply);
			}
			else
			{
				m_networkReply = m_networkManager->put(request, m_file.get());
			}
		}
		else
		{
			if (bFormData)
			{
				m_networkReply = m_networkManager->post(request, httpMultiPart);
				httpMultiPart->setParent(m_networkReply);
			}
			else
			{
				m_networkReply = m_networkManager->post(request, m_file.get());
			}
		}
	}
	else // ftp
	{
		if (bFormData)
		{
			m_networkReply = m_networkManager->put(request, httpMultiPart);
			httpMultiPart->setParent(m_networkReply);
		}
		else
		{
			m_networkReply = m_networkManager->put(request, m_file.get());
		}
	}

	connect(m_networkReply, SIGNAL(finished()), this, SLOT(onFinished()));
	QtCompat::connectErrorSignal(m_networkReply, this, SLOT(onError(QNetworkReply::NetworkError)));
	connect(m_networkManager, SIGNAL(authenticationRequired(QNetworkReply *, QAuthenticator *)),
			SLOT(onAuthenticationRequired(QNetworkReply *, QAuthenticator *)));
#ifndef QT_NO_SSL
	connectSslErrorHandling(m_networkReply);
#endif
	if (m_context->behavior.showProgress)
	{
		connect(m_networkReply, SIGNAL(uploadProgress(qint64, qint64)), this, SLOT(onUploadProgress(qint64, qint64)));
	}

	// Layer2b: transfer timeout via elapsed timer (no-op on Qt >= 5.15)
	QtCompat::startTransferTimer(m_transferElapsed);
	m_throttle->start();
}

void NetworkUploadRequest::onFinished()
{
	if (!m_networkReply)
	{
		setError(ErrorCategory::Network, ErrorCode::InvalidReply, QString("Network error: Invalid reply"));
		emit response(ToFailedResult());
		return;
	}

	CloseFile();

	auto [success, statusCode] = evaluateOutcome();
	if (!success && handleFailure())
		return;

	// Get response header information
	QMap<QByteArray, QByteArray> responseHeaders;
	QByteArray body;
	if (success)
		collectResponse(responseHeaders, body);

	if (success)
		qDebug() << "[NetworkDownloadRequest] Upload completed successfully:" << m_url.toString();
	else
		qDebug() << "[NetworkDownloadRequest] Upload failed:" << m_errorMessage;

	disposeReply();

    m_bytesSent = qMax(m_bytesSent, m_lastSentBytes);
    if (m_result)
    {
        m_result->performance.bytesSent = m_bytesSent;
        m_result->performance.bytesReceived = body.size();
    }

	if (success)
		emit response(ToSuccessResult(body, responseHeaders, statusCode));
	else
		emit response(ToFailedResult(statusCode));
}

void NetworkUploadRequest::onUploadProgress(qint64 iSent, qint64 iTotal)
{
	m_lastSentBytes = iSent;

    // Reset idle timeout on data sent
    if (iSent > 0)
        resetIdleTimer();

	if (m_abortManual)
		return;

	m_throttle->report(iSent, iTotal, [this](qint64 bytes, qint64 total) {
		int progress = bytes * 100 / total;
		if (m_progress < progress)
		{
			m_progress = progress;
			NetworkProgressEvent *event = new NetworkProgressEvent;
			event->isDownload = false;
			event->requestId = m_context->task.id;
			event->batchId = m_context->task.batchId;
			event->transferredBytes = bytes;
			event->totalBytes = total;
			QCoreApplication::postEvent(NetworkRequestManager::globalInstance(), event);
		}
	});
}

void NetworkUploadRequest::CloseFile()
{
	if (m_file)
	{
		if (m_file->isOpen())
		{
			m_file->close();
		}

		m_file.reset();
	}
}
