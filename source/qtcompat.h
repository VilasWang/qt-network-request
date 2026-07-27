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
#include <QObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QThreadPool>

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
		return timer.isValid() && timer.elapsed() > timeoutMs;
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
#else
	constexpr bool kHasTlsV1_3 = false;
#endif

// ============================================================================
// QRecursiveMutex (Qt >= 5.14)
// ============================================================================
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	using Mutex = QRecursiveMutex;
#else
	using Mutex = QMutex;
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

} // namespace QtCompat

