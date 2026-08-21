#pragma once

/// @file qtcompat.h
/// @brief Centralized Qt version compatibility adapters.
///
/// All #if QT_VERSION blocks scattered across the codebase are consolidated here
/// so that adding Qt 6 support (or any future version) requires changes in only
/// one file.

#include <QtGlobal>
#include <QString>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSsl>
#include <QObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QThreadPool>
#include <QList>
#include <QNetworkAccessManager>
#include <QBuffer>
#include <QUuid>

namespace QtCompat
{

// ============================================================================
// transferTimeout (Qt >= 5.15)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	inline void setTransferTimeout(QNetworkRequest& req, int ms)
	{
		if (ms > 0)
			req.setTransferTimeout(ms);
	}
#else
	inline void setTransferTimeout(QNetworkRequest&, int) { /* no-op */ }
#endif

// ============================================================================
// QNetworkReply error signal (renamed errorOccurred in Qt 5.15)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	inline QMetaObject::Connection connectErrorSignal(QNetworkReply* reply, QObject* receiver, const char* slot)
	{
		return QObject::connect(reply, SIGNAL(errorOccurred(QNetworkReply::NetworkError)), receiver, slot);
	}
#else
	inline QMetaObject::Connection connectErrorSignal(QNetworkReply* reply, QObject* receiver, const char* slot)
	{
		return QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), receiver, slot);
	}
#endif

// ============================================================================
// Transfer timeout fallback (Qt < 5.15 uses QElapsedTimer + heartbeat)
// ============================================================================
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	inline void startTransferTimer(QElapsedTimer& timer)
	{
		timer.start();
	}
	inline bool isTransferTimedOut(QElapsedTimer& timer, int timeoutMs)
	{
		// timeoutMs <= 0 means "no timeout", matching Qt >= 5.15 setTransferTimeout()
		// which is a no-op for non-positive values. Without this guard a 0 timeout
		// would evaluate as elapsed() > 0 and abort every request on Qt < 5.15.
		return timeoutMs > 0 && timer.isValid() && timer.elapsed() > timeoutMs;
	}
#else
	inline void startTransferTimer(QElapsedTimer&) { /* no-op: Qt >= 5.15 uses setTransferTimeout */ }
	inline bool isTransferTimedOut(QElapsedTimer&, int) { return false; }
#endif

// ============================================================================
// Http2AllowedAttribute (Qt >= 5.13)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	inline void setHttp2Allowed(QNetworkRequest& req, bool allowed)
	{
		req.setAttribute(QNetworkRequest::Http2AllowedAttribute, allowed);
	}
#else
	inline void setHttp2Allowed(QNetworkRequest&, bool) { /* no-op */ }
#endif

// ============================================================================
// TLS 1.3 support (Qt >= 5.12)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
	constexpr bool kHasTlsV1_3 = true;
	inline QSsl::SslProtocol tlsV1_3OrLater() { return QSsl::TlsV1_3OrLater; }
#else
	constexpr bool kHasTlsV1_3 = false;
	// QSsl::TlsV1_3OrLater does not exist before 5.12; fall back to 1.2.
	inline QSsl::SslProtocol tlsV1_3OrLater() { return QSsl::TlsV1_2OrLater; }
#endif

// ============================================================================
// QRecursiveMutex (Qt >= 5.14)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	using Mutex = QRecursiveMutex;
#else
	// Qt < 5.14 has no QRecursiveMutex. Derive from QMutex and default to
	// Recursive so call sites that relied on QRecursiveMutex keep their
	// recursive-lock behavior (a plain QMutex is non-recursive by default).
	class Mutex : public QMutex
	{
	public:
		Mutex() : QMutex(QMutex::Recursive) {}
	};
#endif

// ============================================================================
// QList::reserve() (Qt >= 5.7). QStringList derives from QList<QString>, so it
// is covered too. Before 5.7 QList had no reserve(); provide a no-op fallback.
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
	template <typename T>
	inline void reserveList(QList<T>& list, int size)
	{
		list.reserve(size);
	}
#else
	template <typename T>
	inline void reserveList(QList<T>&, int) { /* no-op: QList::reserve added in 5.7 */ }
#endif

// ============================================================================
// SkipEmptyParts (Qt >= 5.14 renamed to Qt::SkipEmptyParts)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	constexpr auto kSkipEmptyParts = Qt::SkipEmptyParts;
#else
	constexpr auto kSkipEmptyParts = QString::SkipEmptyParts;
#endif

// ============================================================================
// QThreadPool::tryTake (Qt >= 5.9) + cancel fallback
// Original pattern: tryTake if available, else cancel unconditionally.
// Returns true when the runnable is still running and needs retiring.
// ============================================================================
	inline bool retireIfRunning(QThreadPool* pool, QRunnable* runnable)
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
		return !pool->tryTake(runnable);
#else
		pool->cancel(runnable);  // cancel() exists in all Qt 5.x
		return true;
#endif
	}

// ============================================================================
// sendCustomRequest with a QByteArray body (Qt >= 5.8)
// Before 5.8 sendCustomRequest only accepted a QIODevice* as the body, so the
// QByteArray overload did not exist. Wrap the body in a QBuffer and delete it
// when the reply finishes.
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
	inline QNetworkReply *sendCustomRequest(QNetworkAccessManager *manager,
	                                        const QNetworkRequest &request,
	                                        const QByteArray &verb,
	                                        const QByteArray &data = QByteArray())
	{
		return manager->sendCustomRequest(request, verb, data);
	}
#else
	inline QNetworkReply *sendCustomRequest(QNetworkAccessManager *manager,
	                                        const QNetworkRequest &request,
	                                        const QByteArray &verb,
	                                        const QByteArray &data = QByteArray())
	{
		QBuffer *buffer = nullptr;
		QIODevice *device = nullptr;
		if (!data.isEmpty())
		{
			buffer = new QBuffer;
			buffer->setData(data);
			buffer->open(QIODevice::ReadOnly);
			device = buffer;
		}
		QNetworkReply *reply = manager->sendCustomRequest(request, verb, device);
		if (buffer)
		{
			if (reply)
				QObject::connect(reply, &QNetworkReply::finished, buffer, &QObject::deleteLater);
			else
				delete buffer;
		}
		return reply;
	}
#endif

// ============================================================================
// QUuid::WithoutBraces (Qt >= 5.11)
// Before 5.11 toString() only produced the brace-enclosed form, so strip the
// surrounding braces to get the brace-less UUID string.
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	inline QString createUuidString()
	{
		return QUuid::createUuid().toString(QUuid::WithoutBraces);
	}
#else
	inline QString createUuidString()
	{
		QString s = QUuid::createUuid().toString();  // "{...}" form
		if (s.length() >= 2 && s.startsWith(QLatin1Char('{')) && s.endsWith(QLatin1Char('}')))
			return s.mid(1, s.length() - 2);
		return s;
	}
#endif

} // namespace QtCompat

