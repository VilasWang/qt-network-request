#pragma once

#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QThread>

namespace QtNetworkRequest
{
	// Thread-affine QNetworkAccessManager pool.
	//
	// Each worker thread gets exactly one NAM instance (keyed by
	// threadId + QThread* for ABA-safe double-check).  QThreadPool
	// reuses OS threads across QRunnable invocations, so the same
	// NAM stays alive and TCP keep-alive connections are reused.
	//
	// Lifecycle: created by NetworkRequestManagerPrivate::init(),
	// destroyed by NetworkRequestManagerPrivate::unInitialize()
	// (after QThreadPool::waitForDone() + processEvents()).
	class NetworkAccessManagerPool
	{
	public:
		NetworkAccessManagerPool();
		~NetworkAccessManagerPool();

		// Non-copyable, non-movable
		NetworkAccessManagerPool(const NetworkAccessManagerPool &) = delete;
		NetworkAccessManagerPool &operator=(const NetworkAccessManagerPool &) = delete;

		// Return the thread-affine NAM for the calling thread.
		// Creates a new NAM on first access for a thread;
		// on subsequent calls reapplies global proxy and
		// rebuilds the SharedCookieJar wrapper so that
		// configuration changes take effect immediately.
		QNetworkAccessManager *acquireNam();

		// Delete all NAMs in the pool.  Must be called only when
		// no worker thread is using any NAM (i.e. after
		// QThreadPool::waitForDone() + processEvents()).
		void releaseAll();

		// Returns the number of NAMs currently cached.
		int size() const;

	private:
		struct NamEntry
		{
			QNetworkAccessManager *nam{ nullptr };
			QThread *thread{ nullptr };   // dual-verification against ABA
		};

		QHash<Qt::HANDLE, NamEntry> m_namPool;
		mutable QMutex m_mutex;
	};
}
