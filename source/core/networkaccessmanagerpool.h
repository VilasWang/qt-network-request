#pragma once

#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QThread>
#include <atomic>

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
	//
	// Thread affinity: QNetworkAccessManager is a QObject; it and its
	// children (cookie jar, replies) are affine to the worker thread
	// that created them. They MUST be destroyed on that same thread.
	// Cross-thread delete crashes in the destructor (accesses the
	// already-exited worker's threadData). Therefore shutdown is a
	// two-phase cooperative protocol:
	//   1. setReleasing(true) — signal worker threads to delete their
	//      NAM on run() exit / cleanup QRunnable.
	//   2. releaseAll() — clear any leftover entries (threads that did
	//      not get a cleanup chance); these are leaked to process exit
	//      rather than risked a cross-thread delete.
	class NetworkAccessManagerPool
	{
	public:
		NetworkAccessManagerPool();
		~NetworkAccessManagerPool();

		// Non-copyable, non-movable
		NetworkAccessManagerPool(const NetworkAccessManagerPool &) = delete;
		NetworkAccessManagerPool &operator=(const NetworkAccessManagerPool &) = delete;

		// Return the thread-affine NAM for the calling thread.
		QNetworkAccessManager *acquireNam();

		// Mark pool as shutting down. While true, worker threads destroy
		// their NAM via releaseCurrentThreadNam() (called from run() exit
		// and from shutdown cleanup QRunnables). Called by unInitialize()
		// BEFORE waitForDone() so in-flight run()s delete their NAM.
		void setReleasing(bool b);
		bool isReleasing() const;

		// Destroy the calling thread's NAM if the pool is in releasing mode.
		// MUST be called from the worker thread that owns the NAM — same-
		// thread destruction is the only safe way (QObject affinity).
		void releaseCurrentThreadNam();

		// Clear leftover entries. Called by unInitialize() after waitForDone().
		// NAMs whose owning thread ran a cleanup are already deleted; any
		// residual entries are detached (leaked to process exit) — never
		// cross-thread delete.
		void releaseAll();

		// Returns the number of NAMs currently cached.
		int size() const;

	private:
		struct ThreadNamEntry
		{
			QNetworkAccessManager *nam{ nullptr };
			QThread *thread{ nullptr };   // dual-verification against ABA
		};

		QHash<Qt::HANDLE, ThreadNamEntry> m_namPool;
		mutable QMutex m_mutex;
		std::atomic<bool> m_releasing{ false };
	};
}
