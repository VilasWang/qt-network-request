#pragma once

#include <QObject>
#include <QRunnable>
#include <QMutex>
#include <QTimer>
#include <atomic>
#include "requestcontext.h"
#include "responseresult.h"
#include <QSharedPointer>
#include "qtcompat.h"

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
		int priority() const { return m_priority; }
		const TaskData task() const { return m_task; }

		// End event loop to release task thread, make it idle, and automatically end executing request
		void quit();

	Q_SIGNALS:
		void response(QSharedPointer<QtNetworkRequest::ResponseResult> spResult);
		void exitLoop();
		// Emitted as the very last action of run(), after the worker thread has
		// finished touching this object. The manager uses it to destroy the
		// runnable on the main thread only once run() has fully returned,
		// preventing a use-after-free when a request is cancelled mid-run.
		void finished(quint64 requestId);

	private:
		Q_DISABLE_COPY(NetworkRequestRunnable);
		std::unique_ptr<RequestContext> m_context;
		TaskData m_task;
		int m_priority{ 0 };
		QMetaObject::Connection m_connect;
		std::atomic<bool> m_abort;
		std::atomic<bool> m_isRunning{ false };        // true while run() is executing
		std::atomic<bool> m_responseSent{ false };   // CAS gate: ensures exactly one response is emitted
        mutable QtCompat::Mutex m_mutex;
	};
}
