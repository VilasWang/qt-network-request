#include "networkrequestmanager.h"
#include <atomic>
#include <memory>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QQueue>
#include <QSet>
#include <set>
#include <QThread>
#include <QThreadPool>
#include <QEvent>
#include <QDebug>
#include <QCoreApplication>
#include "qtcompat.h"
#include "networkrequestrunnable.h"
#include "networkreply.h"
#include "networkrequestevent.h"
#include "networkcookiejar.h"
#include "networkaccessmanagerpool.h"

#define DEFAULT_MAX_THREAD_COUNT 8

namespace QtNetworkRequest {
struct PriorityRunnable
{
    int priority{ 0 };
    quint64 sequenceId{ 0 };
    std::shared_ptr<NetworkRequestRunnable> runnable;
    bool operator<(const PriorityRunnable &other) const
    {
        if (priority != other.priority)
            return priority > other.priority;
        return sequenceId < other.sequenceId;
    }
};

// Lightweight QRunnable that deletes the calling worker thread's NAM during
// shutdown. QThreadPool reuses the same OS threads that created the NAMs, so
// this runs on the affine thread — same-thread destruction is safe.
class NamCleanupRunnable : public QRunnable
{
public:
    void run() Q_DECL_OVERRIDE
    {
        NetworkRequestManager::releaseThreadNamOnExit();
    }
};

class NetworkRequestManagerPrivate
{
    Q_DECLARE_PUBLIC(NetworkRequestManager)

public:
    NetworkRequestManagerPrivate();
    ~NetworkRequestManagerPrivate();

private:
    std::shared_ptr<NetworkReply> postRequest(const QUrl &url, quint64 &taskId, quint64 sessionId = (quint64)0);
    std::shared_ptr<NetworkReply> postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &batchId);
    bool sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool blockUserInteraction);

    bool startRunnable(std::shared_ptr<NetworkRequestRunnable> r, bool addToWaitQueueIfNotStarted = true);
    void stopRequest(quint64 taskId);
    void stopBatchRequests(quint64 batchId);
    void stopSessionRequest(quint64 sessionId);
    void stopAllRequest();

    bool releaseRequestThread(quint64 requestId);

    bool setMaxThreadCount(int maxConcurrent);
    int maxThreadCount() const;

    bool isValid(const QUrl &url) const;
    bool isThreadAvailable() const;

    bool addToFailedQueue(std::unique_ptr<RequestContext> context);
    void clearFailQueue();

    std::shared_ptr<NetworkReply> getReply(quint64 requestId, bool shouldRemove = true);
    std::shared_ptr<NetworkReply> getBatchReply(quint64 batchId, bool shouldRemove = true);
    qint64 updateBatchProgress(quint64 requestId, quint64 batchId, qint64 transferredBytes, qint64 totalBytes, bool isDownload);

    quint64 nextRequestId() const;
    quint64 nextBatchId() const;
    quint64 nextSessionId() const;

    void initialize();
    void unInitialize();
    void reset();
    void resetStopFlag();
    void markStopFlag();
    bool isStopped() const;
    bool isSessionStopped(quint64 sessionId) const;

private:
    Q_DISABLE_COPY(NetworkRequestManagerPrivate);
    NetworkRequestManager *q_ptr;

private:
    static std::atomic<quint64> s_requestId;
    static std::atomic<quint64> s_batchId;
    static std::atomic<quint64> s_sessionId;
    std::atomic<bool> m_stopAllFlag;

    mutable QtCompat::Mutex m_mutex;
    QThreadPool *m_threadPool;

    QHash<quint64, std::shared_ptr<NetworkRequestRunnable>> m_runnableMap;
    // Runnables that were cancelled while their run() was still executing on a
    // worker thread. Ownership is held here until the runnable emits finished()
    // (delivered on the main thread after run() returns), at which point it is
    // safe to destroy. Prevents destroying a runnable mid-run (use-after-free).
    QHash<quint64, std::shared_ptr<NetworkRequestRunnable>> m_retiredRunnables;
    // One-to-one. requestId <---> NetworkReply *
    QHash<quint64, std::shared_ptr<NetworkReply>> m_replyMap;
    // One-to-many. batchId <---> NetworkReply *
    QHash<quint64, std::shared_ptr<NetworkReply>> m_batchReplyMap;

    // session
    QMultiMap<quint64, quint64> m_sessionIdToRequestIdMap;
    QSet<quint64> m_stoppedSessionIds;

    // (batchId <---> Total task count)
    QHash<quint64, size_t> m_batchTotalSizeMap;
    // (batchId <----> Task completion count)
    QHash<quint64, size_t> m_batchFinishedSizeMap;

    // (<batchId, <requestId, downloaded bytes>>)
    QHash<quint64, QHash<quint64, qint64>> m_batchDownloadCurrentBytesMap;
    // (batchId <---> Total download bytes)
    QHash<quint64, qint64> m_batchDownloadTotalBytesMap;
    // (<batchId, <requestId, uploaded bytes>>)
    QHash<quint64, QHash<quint64, qint64>> m_batchUploadCurrentBytesMap;
    // (batchId <---> Total upload bytes)
    QHash<quint64, qint64> m_batchUploadTotalBytesMap;

    // Priority queue for pending runnables when all threads are busy
    std::multiset<PriorityRunnable> m_priorityQueue;
    quint64 m_prioritySeq{ 0 };

    // Thread-affine NAM pool (owned, created in init(), destroyed in unInitialize())
    QScopedPointer<NetworkAccessManagerPool> m_namPool;
};
std::atomic<quint64> NetworkRequestManagerPrivate::s_requestId = 0;
std::atomic<quint64> NetworkRequestManagerPrivate::s_batchId = 0;
std::atomic<quint64> NetworkRequestManagerPrivate::s_sessionId = 0;

NetworkRequestManagerPrivate::NetworkRequestManagerPrivate()
    : m_stopAllFlag(false), m_threadPool(new QThreadPool), q_ptr(nullptr)
{
}

NetworkRequestManagerPrivate::~NetworkRequestManagerPrivate()
{
    qDebug() << "[QMultiThreadNetwork] Runnable size: " << m_runnableMap.size();

    unInitialize();
    m_threadPool->deleteLater();
}

void NetworkRequestManagerPrivate::initialize()
{
    // Register meta types for signal/slot connections across threads
    qRegisterMetaType<QMap<QByteArray, QByteArray>>("QMap<QByteArray, QByteArray>");
    qRegisterMetaType<QSharedPointer<QtNetworkRequest::ResponseResult>>("QSharedPointer<QtNetworkRequest::ResponseResult>");

    int idealThreadCount = QThread::idealThreadCount();
    if (-1 != idealThreadCount)
    {
        m_threadPool->setMaxThreadCount(idealThreadCount);
    }
    else
    {
        m_threadPool->setMaxThreadCount(DEFAULT_MAX_THREAD_COUNT);
    }

    // Create the thread-affine NAM pool
    m_namPool.reset(new NetworkAccessManagerPool());

    // To add something intialize...
}

void NetworkRequestManagerPrivate::unInitialize()
{
    stopAllRequest();  // internally calls reset() — reply maps already cleared

    // Phase 1: signal NAM pool shutdown. In-flight run()s will delete their
    // thread-affine NAM on exit (same-thread destruction — safe, required by
    // QObject affinity: NAM/cookie-jar/replies are affine to the worker).
    if (m_namPool)
        m_namPool->setReleasing(true);

    m_threadPool->clear();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!m_threadPool->waitForDone(1000))
    {
        qDebug() << "[QMultiThreadNetwork] ThreadPool waitForDone failed!";
    }

    // Phase 2: dispatch cleanup runnables so each surviving pooled NAM's
    // owning worker thread deletes its own NAM (same-thread). QThreadPool
    // reuses the same OS threads that created the NAMs, so cleanup runs on
    // the affine thread. Cross-thread delete would crash (0xC0000005).
    if (m_namPool && m_namPool->size() > 0)
    {
        const int n = m_threadPool->maxThreadCount();
        for (int i = 0; i < n; ++i)
            m_threadPool->start(new NamCleanupRunnable);
        m_threadPool->waitForDone(2000);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // Phase 3: detach any residual entries (threads that didn't get a
    // cleanup chance) — never cross-thread delete.
    if (m_namPool)
        m_namPool->releaseAll();

    // All worker run()s have now completed (waitForDone above), so any retired
    // runnables that never had their finished() drained are safe to destroy.
    {
        QMutexLocker locker(&m_mutex);
        m_retiredRunnables.clear();
    }
}

void NetworkRequestManagerPrivate::reset()
{
    // Drain pending events before clearing reply maps: worker threads may
    // have posted ReplyResultEvents destined for NetworkReply objects held
    // in m_replyMap.  Delivering them here while targets are still alive
    // avoids a use-after-free crash (0xC0000005) on the subsequent
    // processEvents() call in unInitialize().
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    QMutexLocker locker(&m_mutex);

    m_batchTotalSizeMap.clear();
    m_batchFinishedSizeMap.clear();
    m_batchDownloadCurrentBytesMap.clear();
    m_batchDownloadTotalBytesMap.clear();
    m_batchUploadCurrentBytesMap.clear();
    m_batchUploadTotalBytesMap.clear();

    m_runnableMap.clear();
    m_replyMap.clear();
    m_batchReplyMap.clear();

    m_sessionIdToRequestIdMap.clear();
    m_stoppedSessionIds.clear();
}

void NetworkRequestManagerPrivate::resetStopFlag()
{
    if (m_stopAllFlag.load(std::memory_order_relaxed)) // Inter-thread synchronization reads are faster than writes
    {
        m_stopAllFlag.store(false, std::memory_order_release);
    }
}

void NetworkRequestManagerPrivate::markStopFlag()
{
    if (!m_stopAllFlag.load(std::memory_order_relaxed)) // Inter-thread synchronization reads are faster than writes
    {
        m_stopAllFlag.store(true, std::memory_order_release);
    }
}

bool NetworkRequestManagerPrivate::isStopped() const
{
    return m_stopAllFlag.load(std::memory_order_acquire);
}

bool NetworkRequestManagerPrivate::isSessionStopped(quint64 sessionId) const
{
    if (m_stoppedSessionIds.end() != m_stoppedSessionIds.find(sessionId))
    {
        return true;
    }
    return false;
}

void NetworkRequestManagerPrivate::stopRequest(quint64 taskId)
{
    if (taskId == 0)
        return;

    auto rsp = QSharedPointer<ResponseResult>::create();
    std::shared_ptr<NetworkReply> reply = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        reply = m_replyMap.take(taskId);

        // Check running map
        if (m_runnableMap.contains(taskId))
        {
            std::shared_ptr<NetworkRequestRunnable> r = m_runnableMap.take(taskId);
            if (r.get())
            {
                rsp->task = r->task();

                if (QtCompat::retireIfRunning(m_threadPool, r.get()))
                {
                    r->quit();
                    // run() is still executing on a worker thread. Keep the
                    // runnable alive until it emits finished() (delivered on
                    // the main thread after run() returns) — dropping it here
                    // would destroy it mid-run (use-after-free).
                    m_retiredRunnables.insert(r->requestId(), r);
                }
                // r will be naturally released when leaving scope for the
                // not-started case; running runnables are owned by
                // m_retiredRunnables until finished() fires.
            }
        }
        // Check priority queue
        for (auto it = m_priorityQueue.begin(); it != m_priorityQueue.end(); ++it)
        {
            if (it->runnable && it->runnable->requestId() == taskId)
            {
                rsp->task = it->runnable->task();
                m_priorityQueue.erase(it);
                break;
            }
        }
    }

    if (reply.get())
    {
        rsp->error.category = ErrorCategory::Cancelled;
        rsp->error.code = ErrorCode::OperationCancelled;
        rsp->error.message = QString("Operation canceled (id: %1)").arg(taskId);
        rsp->body = QString("Operation canceled (id: %1)").arg(taskId).toUtf8();
        rsp->task.endTime = QDateTime::currentDateTime();

        reply->replyResult(rsp, true);
    }
}

void NetworkRequestManagerPrivate::stopBatchRequests(quint64 batchId)
{
    if (batchId == 0)
        return;

    std::shared_ptr<NetworkReply> reply = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        reply = m_batchReplyMap.take(batchId);

        // qDebug() << "Runnable[Before]: " << m_runnableMap.size();
        for (auto iter = m_runnableMap.begin(); iter != m_runnableMap.end();)
        {
            std::shared_ptr<NetworkRequestRunnable> r = iter.value();
            if (r.get() && r->batchId() == batchId)
            {
                if (QtCompat::retireIfRunning(m_threadPool, r.get()))
                {
                    r->quit();
                    // Still running: keep alive until finished() (see stopRequest).
                    m_retiredRunnables.insert(r->requestId(), r);
                }
                iter = m_runnableMap.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
        // qDebug() << "Runnable[After]: " << m_runnableMap.size();

        // Remove from priority queue
        for (auto it = m_priorityQueue.begin(); it != m_priorityQueue.end();)
        {
            if (it->runnable && it->runnable->batchId() == batchId)
                it = m_priorityQueue.erase(it);
            else
                ++it;
        }

        if (m_batchTotalSizeMap.contains(batchId))
        {
            m_batchTotalSizeMap.remove(batchId);
        }
        if (m_batchFinishedSizeMap.contains(batchId))
        {
            m_batchFinishedSizeMap.remove(batchId);
        }
        if (m_batchDownloadCurrentBytesMap.contains(batchId))
        {
            m_batchDownloadCurrentBytesMap.remove(batchId);
        }
        if (m_batchDownloadTotalBytesMap.contains(batchId))
        {
            m_batchDownloadTotalBytesMap.remove(batchId);
        }
        if (m_batchUploadCurrentBytesMap.contains(batchId))
        {
            m_batchUploadCurrentBytesMap.remove(batchId);
        }
        if (m_batchUploadTotalBytesMap.contains(batchId))
        {
            m_batchUploadTotalBytesMap.remove(batchId);
        }
    }

    if (reply.get())
    {
        auto rsp = QSharedPointer<ResponseResult>::create();
        rsp->task.batchId = batchId;
        rsp->error.category = ErrorCategory::Cancelled;
        rsp->error.code = ErrorCode::OperationCancelled;
        rsp->error.message = QString("Operation canceled (Batch id: %1)").arg(batchId);
        rsp->body = QString("Operation canceled (Batch id: %1)").arg(batchId).toUtf8();
        rsp->task.endTime = QDateTime::currentDateTime();

        reply->replyResult(rsp, true);
    }
}

void NetworkRequestManagerPrivate::stopSessionRequest(quint64 sessionId)
{
    if (sessionId == 0)
        return;

    QMutexLocker locker(&m_mutex);
    m_stoppedSessionIds.insert(sessionId);
    for (auto iter = m_runnableMap.begin(); iter != m_runnableMap.end();)
    {
        std::shared_ptr<NetworkRequestRunnable> r = iter.value();
        if (r.get() && r->sessionId() == sessionId)
        {
            if (QtCompat::retireIfRunning(m_threadPool, r.get()))
            {
                r->quit();
                // Still running: keep alive until finished() (see stopRequest).
                m_retiredRunnables.insert(r->requestId(), r);
            }
            iter = m_runnableMap.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    // Remove from priority queue
    for (auto it = m_priorityQueue.begin(); it != m_priorityQueue.end();)
    {
        if (it->runnable && it->runnable->sessionId() == sessionId)
            it = m_priorityQueue.erase(it);
        else
            ++it;
    }

    QList<quint64> uiRequestIds = m_sessionIdToRequestIdMap.values(sessionId);
    for (quint64 &requestId : uiRequestIds)
    {
        if (m_replyMap.contains(requestId))
        {
            m_replyMap.remove(requestId);
        }
    }
}

void NetworkRequestManagerPrivate::stopAllRequest()
{
    if (isStopped())
        return;

    markStopFlag();

    {
        QMutexLocker locker(&m_mutex);

        for (auto iter = m_runnableMap.cbegin(); iter != m_runnableMap.cend(); ++iter)
        {
            std::shared_ptr<NetworkRequestRunnable> r = iter.value();
            if (r.get())
            {
                if (QtCompat::retireIfRunning(m_threadPool, r.get()))
                {
                    r->quit();
                    // Still running: keep alive until finished() (see stopRequest).
                    m_retiredRunnables.insert(r->requestId(), r);
                }
            }
        }
        m_runnableMap.clear();
        m_priorityQueue.clear();
    }
    reset();
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::postRequest(const QUrl &url, quint64 &requestId, quint64 sessionId)
{
    if (isValid(url))
    {
        requestId = nextRequestId();
        std::unique_ptr<TaskData> task = std::make_unique<TaskData>();
        task->id = requestId;
        task->sessionId = sessionId;
        std::shared_ptr<NetworkReply> pReply = std::make_shared<NetworkReply>(std::move(task));
        m_replyMap.insert(requestId, pReply);

        if (sessionId > 0)
        {
            m_sessionIdToRequestIdMap.insert(sessionId, requestId);
        }

        return pReply;
    }
    return nullptr;
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &batchId)
{
    if (tasks.empty())
        return nullptr;

    batchId = nextBatchId();
    m_batchTotalSizeMap[batchId] = tasks.size();

    std::unique_ptr<TaskData> task = std::make_unique<TaskData>();
    task->batchId = batchId;
    std::shared_ptr<NetworkReply> pReply = std::make_shared<NetworkReply>(std::move(task));
    m_batchReplyMap.insert(batchId, pReply);

    for (auto &context : tasks)
    {
        if (!context)
        {
            continue;
        }
        context->task.batchId = batchId;
        context->task.id = nextRequestId();
        context->task.createTime = QDateTime::currentDateTime();

        Q_Q(NetworkRequestManager);
        q->startAsRunnable(std::move(context));
    }

    return pReply;
}

bool NetworkRequestManagerPrivate::sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool blockUserInteraction)
{
    if (!context || !isValid(context->url))
        return false;

    context->task.id = nextRequestId();
    context->task.createTime = QDateTime::currentDateTime();

    QEventLoop eventloop;

    std::shared_ptr<NetworkRequestRunnable> r = std::make_shared<NetworkRequestRunnable>(std::move(context));
    QObject::connect(r.get(), &NetworkRequestRunnable::response, &eventloop, [&](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
        if (callback)
            callback(rsp);
        releaseRequestThread(rsp->task.id);
        eventloop.quit(); });

    if (!startRunnable(r, false))
    {
        r.reset();
        return false;
    }
    if (blockUserInteraction)
        eventloop.exec(QEventLoop::ExcludeUserInputEvents);
    else
        eventloop.exec();
    return true;
}

quint64 NetworkRequestManagerPrivate::nextRequestId() const
{
    return s_requestId.fetch_add(1, std::memory_order_relaxed) + 1;
}

quint64 NetworkRequestManagerPrivate::nextBatchId() const
{
    return s_batchId.fetch_add(1, std::memory_order_relaxed) + 1;
}

quint64 NetworkRequestManagerPrivate::nextSessionId() const
{
    return s_sessionId.fetch_add(1, std::memory_order_relaxed) + 1;
}

bool NetworkRequestManagerPrivate::startRunnable(std::shared_ptr<NetworkRequestRunnable> r, bool addToWaitQueueIfNotStarted)
{
    if (!r.get())
        return false;

    // Route the runnable's end-of-run() notification to the manager so its
    // destruction always happens on the main thread after run() has returned.
    Q_Q(NetworkRequestManager);
    QObject::connect(r.get(), &NetworkRequestRunnable::finished,
                     q, &NetworkRequestManager::onRunnableFinished);

    try
    {
        if (addToWaitQueueIfNotStarted)
        {
            if (m_threadPool->tryStart(r.get()))
            {
                QMutexLocker locker(&m_mutex);
                m_runnableMap.insert(r->requestId(), r);
                return true;
            }
            // All threads busy: add to priority queue
            QMutexLocker locker(&m_mutex);
            PriorityRunnable pr;
            pr.priority = r->priority();
            pr.sequenceId = ++m_prioritySeq;
            pr.runnable = r;
            m_priorityQueue.insert(pr);
            qDebug() << "[QMultiThreadNetwork] Queued request (priority:" << pr.priority << ")";
            return true;
        }
        else
        {
            if (!m_threadPool->tryStart(r.get()))
                return false;
            QMutexLocker locker(&m_mutex);
            m_runnableMap.insert(r->requestId(), r);
            return true;
        }
    }
    catch (const std::exception &e)
    {
        qCritical() << "[QMultiThreadNetwork] startRunnable() exception:" << QString::fromUtf8(e.what());
    }
    catch (...)
    {
        qCritical() << "[QMultiThreadNetwork] startRunnable() unknown exception";
    }

    return false;
}

bool NetworkRequestManagerPrivate::setMaxThreadCount(int maxConcurrent)
{
    bool result = false;
    if (maxConcurrent >= 1 && maxConcurrent <= 100 && m_threadPool)
    {
        qDebug() << "[QMultiThreadNetwork] ThreadPool maxThreadCount: " << maxConcurrent;
        m_threadPool->setMaxThreadCount(maxConcurrent);
        result = true;
    }
    return result;
}

int NetworkRequestManagerPrivate::maxThreadCount() const
{
    if (m_threadPool)
    {
        return m_threadPool->maxThreadCount();
    }
    return -1;
}

bool NetworkRequestManagerPrivate::isThreadAvailable() const
{
    if (m_threadPool)
    {
        return (m_threadPool->activeThreadCount() < m_threadPool->maxThreadCount());
    }
    return false;
}

bool NetworkRequestManagerPrivate::isValid(const QUrl &url) const
{
    return (url.isValid());
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::getReply(quint64 requestId, bool shouldRemove)
{
    QMutexLocker locker(&m_mutex);
    if (m_replyMap.contains(requestId))
    {
        if (shouldRemove)
        {
            return m_replyMap.take(requestId);
        }
        else
        {
            return m_replyMap.value(requestId);
        }
    }
    qDebug() << QString("%1 failed! Id: ").arg(__FUNCTION__) << requestId;
    return nullptr;
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::getBatchReply(quint64 batchId, bool shouldRemove)
{
    QMutexLocker locker(&m_mutex);
    if (m_batchReplyMap.contains(batchId))
    {
        if (shouldRemove)
        {
            return m_batchReplyMap.take(batchId);
        }
        else
        {
            return m_batchReplyMap.value(batchId);
        }
    }
    return nullptr;
}

qint64 NetworkRequestManagerPrivate::updateBatchProgress(quint64 requestId, quint64 batchId, qint64 transferredBytes, qint64 totalBytes, bool isDownload)
{
    Q_UNUSED(totalBytes);
    // postEvent() calls are all in main thread, no need to lock

    // Bytes increased for this request task compared to last time (download/upload)
    quint64 increased = 0;
    quint64 accumulatedBytes = 0;
    if (transferredBytes == 0)
    {
        if (isDownload)
        {
            return m_batchDownloadTotalBytesMap[batchId];
        }
        else
        {
            return m_batchUploadTotalBytesMap[batchId];
        }
    }

    if (isDownload)
    {
        const QHash<quint64, qint64> &mapReqId2Bytes = m_batchDownloadCurrentBytesMap.value(batchId);
        if (mapReqId2Bytes.contains(requestId))
        {
            qint64 curBytes = mapReqId2Bytes.value(requestId);
            if (transferredBytes > curBytes)
            {
                increased = transferredBytes - curBytes;
            }
        }
        else
        {
            increased = transferredBytes;
        }
        m_batchDownloadCurrentBytesMap[batchId][requestId] = transferredBytes;

        accumulatedBytes = m_batchDownloadTotalBytesMap.value(batchId) + increased;
        m_batchDownloadTotalBytesMap[batchId] = accumulatedBytes;
    }
    else
    {
        const QHash<quint64, qint64> &mapReqId2Bytes = m_batchUploadCurrentBytesMap.value(batchId);
        if (mapReqId2Bytes.contains(requestId))
        {
            qint64 curBytes = mapReqId2Bytes.value(requestId);
            if (transferredBytes > curBytes)
            {
                increased = transferredBytes - curBytes;
            }
        }
        else
        {
            increased = transferredBytes;
        }
        m_batchUploadCurrentBytesMap[batchId][requestId] = transferredBytes;

        accumulatedBytes = m_batchUploadTotalBytesMap.value(batchId) + increased;
        m_batchUploadTotalBytesMap[batchId] = accumulatedBytes;
    }

    return accumulatedBytes;
}

bool NetworkRequestManagerPrivate::releaseRequestThread(quint64 requestId)
{
    QMutexLocker locker(&m_mutex);
    if (m_runnableMap.contains(requestId))
    {
        std::shared_ptr<NetworkRequestRunnable> r = m_runnableMap.take(requestId);
        if (r.get())
        {
            // The response is delivered on the main thread while run() is still
            // executing on a worker thread (blocked in its event loop). Dropping
            // the last reference here would destroy the runnable mid-run() and
            // race with run()'s tail (cleanup + finished()) — a use-after-free.
            // Keep it alive in m_retiredRunnables until finished() is delivered
            // on the main thread (onRunnableFinished), exactly as the stop paths
            // do; quit() below wakes run() so it returns and emits finished().
            m_retiredRunnables.insert(requestId, r);
            r->quit();
        }
    }
    // Dequeue next pending runnable from priority queue
    if (!m_priorityQueue.empty())
    {
        auto it = m_priorityQueue.begin();
        PriorityRunnable pr = *it;
        m_priorityQueue.erase(it);
        locker.unlock();

        m_threadPool->start(pr.runnable.get());
        locker.relock();
        m_runnableMap.insert(pr.runnable->requestId(), pr.runnable);
        qDebug() << "[QMultiThreadNetwork] Dequeued request (priority:" << pr.priority << ")";
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
std::atomic<bool> NetworkRequestManager::s_isInitialized = false;
std::atomic<bool> NetworkRequestManager::s_isUninitializing = false;
ProxyConfig NetworkRequestManager::s_globalProxy{};
QScopedPointer<QNetworkCookieJar> NetworkRequestManager::s_cookieJar;
#ifndef QT_NO_SSL
SslConfig NetworkRequestManager::s_globalSslConfig = SslConfig::secureDefault();
QMutex NetworkRequestManager::s_globalSslConfigMutex;
#endif

NetworkRequestManager::NetworkRequestManager(QObject *parent)
    : QObject(parent), d_ptr(new NetworkRequestManagerPrivate)
{
    Q_D(NetworkRequestManager);
    d->q_ptr = this;
    // qDebug() << "[QMultiThreadNetwork] Thread : " << QThread::currentThreadId();
}

NetworkRequestManager::~NetworkRequestManager()
{
    d_ptr.reset();
}

NetworkRequestManager *NetworkRequestManager::globalInstance()
{
    static NetworkRequestManager s_instance;
    return &s_instance;
}

void NetworkRequestManager::initialize()
{
    if (!s_isInitialized)
    {
        NetworkRequestManager::globalInstance()->init();
        s_isInitialized = true;
    }
}

void NetworkRequestManager::unInitialize()
{
    if (s_isInitialized)
    {
        s_isUninitializing = true;
        NetworkRequestManager::globalInstance()->fini();
        s_isInitialized = false;
        s_isUninitializing = false;
    }
}

bool NetworkRequestManager::isInitialized()
{
    return s_isInitialized && !s_isUninitializing;
}

void NetworkRequestManager::setGlobalProxy(const ProxyConfig &config)
{
    s_globalProxy = config;
}

const ProxyConfig &NetworkRequestManager::globalProxy()
{
    return s_globalProxy;
}

#ifndef QT_NO_SSL
void NetworkRequestManager::setGlobalSslConfig(const SslConfig &config)
{
    // Normalize: global fields must never remain Inherit (no higher level to
    // inherit from). Replace any Inherit with the secure-default value.
    SslConfig normalized = config;
    const SslConfig def = SslConfig::secureDefault();
    if (normalized.peerVerifyMode == SslConfig::PeerVerifyMode::Inherit)
        normalized.peerVerifyMode = def.peerVerifyMode;
    if (normalized.minProtocol == SslConfig::TlsProtocol::Inherit)
        normalized.minProtocol = def.minProtocol;
    if (normalized.ignoreSslErrorsPolicy == SslConfig::IgnorePolicy::Inherit)
        normalized.ignoreSslErrorsPolicy = def.ignoreSslErrorsPolicy;
    if (normalized.caPolicy == SslConfig::CaPolicy::Inherit)
        normalized.caPolicy = def.caPolicy;

    QMutexLocker locker(&s_globalSslConfigMutex);
    s_globalSslConfig = normalized;
}

SslConfig NetworkRequestManager::globalSslConfig()
{
    // Return by value so callers (worker threads) hold an independent copy
    // and never dereference the global while another thread mutates it.
    QMutexLocker locker(&s_globalSslConfigMutex);
    return s_globalSslConfig;
}
#endif

void NetworkRequestManager::setCookieStoragePath(const QString &path)
{
    PersistentCookieJar *jar = new PersistentCookieJar(path);
    s_cookieJar.reset(jar);
}

QString NetworkRequestManager::cookieStoragePath()
{
    auto *jar = qobject_cast<PersistentCookieJar*>(s_cookieJar.data());
    return jar ? jar->filePath() : QString();
}

QNetworkCookieJar *NetworkRequestManager::cookieJar()
{
    return s_cookieJar.data();
}

QNetworkAccessManager *NetworkRequestManager::acquireThreadNam()
{
    auto *d = globalInstance()->d_func();
    return d->m_namPool ? d->m_namPool->acquireNam() : nullptr;
}

void NetworkRequestManager::releaseThreadNamOnExit()
{
    auto *inst = globalInstance();
    if (!inst)
        return;
    auto *d = inst->d_func();
    if (d && d->m_namPool)
        d->m_namPool->releaseCurrentThreadNam();
}

void NetworkRequestManager::init()
{
    Q_D(NetworkRequestManager);
    d->initialize();
}

void NetworkRequestManager::fini()
{
    Q_D(NetworkRequestManager);
    d->unInitialize();
}

std::shared_ptr<NetworkReply> NetworkRequestManager::postRequest(std::unique_ptr<RequestContext> request)
{
    if (!request)
    {
        return nullptr;
    }
    if (!NetworkRequestManager::isInitialized())
    {
        qDebug() << "[QMultiThreadNetwork] You must call NetworkRequestManager::initialize() before any request.";
        return nullptr;
    }

    Q_D(NetworkRequestManager);
    d->resetStopFlag();

    std::shared_ptr<NetworkReply> pReply = d->postRequest(request->url, request->task.id, request->task.sessionId);
    if (pReply)
    {
        request->task.createTime = QDateTime::currentDateTime();
        startAsRunnable(std::move(request));
    }
    return pReply;
}

std::shared_ptr<NetworkReply> NetworkRequestManager::postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &batchId)
{
    if (!NetworkRequestManager::isInitialized())
    {
        qDebug() << "[QMultiThreadNetwork] You must call NetworkRequestManager::initialize() before any request.";
        return nullptr;
    }

    Q_D(NetworkRequestManager);
    d->resetStopFlag();

    batchId = 0;
    if (!tasks.empty())
    {
        std::shared_ptr<NetworkReply> pReply = d->postBatchRequest(std::move(tasks), batchId);
        return pReply;
    }
    return nullptr;
}

bool NetworkRequestManager::sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool blockUserInteraction)
{
    if (!NetworkRequestManager::isInitialized())
    {
        qDebug() << "[QMultiThreadNetwork] You must call NetworkRequestManager::initialize() before any request.";
        return false;
    }
    Q_D(NetworkRequestManager);
    return d->sendRequest(std::move(context), callback, blockUserInteraction);
}

void NetworkRequestManager::stopRequest(quint64 taskId)
{
    Q_D(NetworkRequestManager);
    d->stopRequest(taskId);
}

void NetworkRequestManager::stopBatchRequests(quint64 batchId)
{
    Q_D(NetworkRequestManager);
    d->stopBatchRequests(batchId);
}

void NetworkRequestManager::stopSessionRequest(quint64 sessionId)
{
    Q_D(NetworkRequestManager);
    d->stopSessionRequest(sessionId);
}

void NetworkRequestManager::stopAllRequest()
{
    Q_D(NetworkRequestManager);
    d->stopAllRequest();
}

quint64 NetworkRequestManager::nextSessionId()
{
    Q_D(NetworkRequestManager);
    return d->nextSessionId();
}

bool NetworkRequestManager::startAsRunnable(std::unique_ptr<RequestContext> context)
{
    std::shared_ptr<NetworkRequestRunnable> r = std::make_shared<NetworkRequestRunnable>(std::move(context));
    connect(r.get(), &NetworkRequestRunnable::response, this, &NetworkRequestManager::onResponse);

    Q_D(NetworkRequestManager);
    if (!d->startRunnable(r))
    {
        qDebug() << "[QMultiThreadNetwork] startRunnable() failed!";

        r.reset();
        return false;
    }
    return true;
}

bool NetworkRequestManager::setMaxThreadCount(int maxConcurrent)
{
    Q_D(NetworkRequestManager);
    return d->setMaxThreadCount(maxConcurrent);
}

int NetworkRequestManager::maxThreadCount()
{
    Q_D(NetworkRequestManager);
    return d->maxThreadCount();
}

bool NetworkRequestManager::event(QEvent *event)
{
    if (event->type() == NetworkEvent::NetworkProgress)
    {
        Q_D(NetworkRequestManager);
        if (d->isStopped())
        {
            return true;
        }

        NetworkProgressEvent *evtProgress = static_cast<NetworkProgressEvent *>(event);
        if (nullptr != evtProgress)
        {
            updateProgress(evtProgress->requestId,
                           evtProgress->batchId,
                           evtProgress->transferredBytes,
                           evtProgress->totalBytes,
                           evtProgress->isDownload);
        }
        return true;
    }

    return QObject::event(event);
}

void NetworkRequestManager::updateProgress(quint64 requestId, quint64 batchId, qint64 transferredBytes, qint64 totalBytes, bool isDownload)
{
    Q_D(NetworkRequestManager);
    if (requestId == 0)
        return;

    // Find the reply for the single request
    std::shared_ptr<NetworkReply> singleReply = d->getReply(requestId, false); // Do not remove from map

    if (singleReply)
    {
        if (isDownload)
        {
            emit singleReply->downloadProgress(transferredBytes, totalBytes);
        }
        else
        {
            emit singleReply->uploadProgress(transferredBytes, totalBytes);
        }
    }

    if (batchId > 0) // Batch request
    {
        // Find the reply for the batch
        std::shared_ptr<NetworkReply> batchReply = d->getBatchReply(batchId, false); // Do not remove from map
        if (batchReply)
        {
            quint64 totalBatchBytes = d->updateBatchProgress(requestId, batchId, transferredBytes, totalBytes, isDownload);
            if (isDownload)
            {
                emit batchReply->batchDownloadProgress(totalBatchBytes);
            }
            else
            {
                emit batchReply->batchUploadProgress(totalBatchBytes);
            }
        }
    }
}

void NetworkRequestManager::onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
{
    Q_ASSERT(QThread::currentThread() == NetworkRequestManager::globalInstance()->thread());
    Q_D(NetworkRequestManager);
    if (d->isStopped())
        return;
    if (d->isSessionStopped(rsp->task.sessionId))
        return;

    rsp->performance.durationMs = rsp->task.startTime.msecsTo(rsp->task.endTime);
    try
    {
        // 2. Notify user of results
        std::shared_ptr<NetworkReply> pReply;
        bool isDestroyed = true;
        auto batchId = rsp->task.batchId;
        if (batchId == 0)
        {
            pReply = d->getReply(rsp->task.id, isDestroyed);
        }
        else if (batchId > 0) // Batch task
        {
            size_t sizeFinished = 0;
            size_t sizeTotal = 0;
            {
                QMutexLocker locker(&d->m_mutex);
                sizeTotal = d->m_batchTotalSizeMap.value(rsp->task.batchId);
                if (sizeTotal > 0)
                {
                    sizeFinished = d->m_batchFinishedSizeMap.value(batchId);
                    d->m_batchFinishedSizeMap[batchId] = ++sizeFinished;

                    if (sizeFinished == sizeTotal)
                    {
                        d->m_batchTotalSizeMap.remove(batchId);
                        d->m_batchFinishedSizeMap.remove(batchId);
                    }
                }
            }

            if (rsp->isSuccess())
            {
                if (sizeFinished < sizeTotal) // Still have requests not completed
                {
                    isDestroyed = false;
                }
            }
            else // Batch task failed
            {
                if (!rsp->task.abortBatchOnFailed && (sizeFinished < sizeTotal))
                {
                    isDestroyed = false;
                }
            }
            pReply = d->getBatchReply(batchId, isDestroyed);
        }

        if (pReply.get())
        {
            pReply->replyResult(rsp, isDestroyed);
            if (batchId > 0 && isDestroyed)
            {
                qDebug() << QString("[QMultiThreadNetwork] Batch request finished! Id: %1").arg(batchId);
                emit batchRequestFinished(batchId, rsp->isSuccess());
            }
        }

        // 3. If batch task failed and bAbortBatchWhileOneFailed is specified, stop tasks in this batch
        if (batchId > 0 && !rsp->isSuccess() && rsp->task.abortBatchOnFailed)
        {
            d->stopBatchRequests(batchId);
        }

        // 4. Release task thread to make it idle
        d->releaseRequestThread(rsp->task.id);
    }
    catch (const std::exception &e)
    {
        qCritical() << "NetworkRequestManager::onResponse() exception:" << QString::fromUtf8(e.what());
    }
    catch (...)
    {
        qCritical() << "NetworkRequestManager::onResponse() unknown exception";
    }
}

void NetworkRequestManager::onRunnableFinished(quint64 requestId)
{
    // Runs on the main thread once the runnable's run() has fully returned
    // (finished() is emitted as run()'s last action, delivered via a queued
    // connection). This is the safe point to release the runnable — for both
    // normally-completed and cancelled-while-running requests.
    Q_D(NetworkRequestManager);
    QMutexLocker locker(&d->m_mutex);
    d->m_retiredRunnables.remove(requestId);
}

} // namespace QtNetworkRequest
