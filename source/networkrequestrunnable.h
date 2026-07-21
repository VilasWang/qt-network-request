#pragma once

#include <QObject>
#include <QRunnable>
#include <QMutex>
#include <QTimer>
#include <atomic>
#include "networkrequestdefs.h"
#include <QSharedPointer>

namespace QtNetworkRequest
{
	class NetworkRequestRunnable : public QObject, public QRunnable
	{
		Q_OBJECT

	public:
		explicit NetworkRequestRunnable(std::unique_ptr<RequestContext> request, QObject *parent = 0);
		~NetworkRequestRunnable();

		// Automatically called after executing QThreadPool::start(QRunnable) or QThreadPool::tryStart(QRunnable)
		virtual void run() Q_DECL_OVERRIDE;

		quint64 requestId() const;
		quint64 batchId() const;
		quint64 sessionId() const;
		int priority() const { return m_nPriority; }
		const TaskData task() const { return m_task; }

		// End event loop to release task thread, make it idle, and automatically end executing request
		void quit();

	Q_SIGNALS:
		void response(QSharedPointer<QtNetworkRequest::ResponseResult> spResult);
		void exitLoop();

	private:
		Q_DISABLE_COPY(NetworkRequestRunnable);
		std::unique_ptr<RequestContext> m_context;
		TaskData m_task;
		int m_nPriority{ 0 };
		QMetaObject::Connection m_connect;
		std::atomic<bool> m_bAbort;
		std::atomic<bool> m_bRunning{ false };        // true while run() is executing
		std::atomic<bool> m_responseSent{ false };   // CAS gate: ensures exactly one response is emitted
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        mutable QRecursiveMutex m_mutex;
#else
        mutable QMutex m_mutex;
#endif
	};
}
