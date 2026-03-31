#include "networkrequestmanager.h"
#include <atomic>
#include <memory>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>
#include <QQueue>
#include <QThread>
#include <QThreadPool>
#include <QEvent>
#include <QDebug>
#include <QCoreApplication>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
#include <QRecursiveMutex>
#endif
#include "networkrequestrunnable.h"
#include "networkreply.h"
#include "networkrequestevent.h"

using namespace QtNetworkRequest;
#define DEFAULT_MAX_THREAD_COUNT 8

class NetworkRequestManagerPrivate
{
    Q_DECLARE_PUBLIC(NetworkRequestManager)

public:
    NetworkRequestManagerPrivate();
    ~NetworkRequestManagerPrivate();

private:
    std::shared_ptr<NetworkReply> postRequest(const QUrl &url, quint64 &uiTaskId, quint64 uiSessionId = (quint64)0);
    std::shared_ptr<NetworkReply> postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &uiBatchId);
    bool sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool bBlockUserInteraction);

    bool startRunnable(std::shared_ptr<NetworkRequestRunnable> r, bool bAddToWaitQueueIfNotStart = true);
    void stopRequest(quint64 uiTaskId);
    void stopBatchRequests(quint64 uiBatchId);
    void stopSessionRequest(quint64 uiSessionId);
    void stopAllRequest();

    bool releaseRequestThread(quint64 uiId);

    bool setMaxThreadCount(int iMax);
    int maxThreadCount() const;

    bool isValid(const QUrl &url) const;
    bool isThreadAvailable() const;

    bool addToFailedQueue(std::unique_ptr<RequestContext> context);
    void clearFailQueue();

    std::shared_ptr<NetworkReply> getReply(quint64 uiId, bool bRemove = true);
    std::shared_ptr<NetworkReply> getBatchReply(quint64 uiBatchId, bool bRemove = true);
    qint64 updateBatchProgress(quint64 uiId, quint64 uiBatchId, qint64 iBytes, qint64 iTotalBytes, bool bDownload);

    quint64 nextRequestId() const;
    quint64 nextBatchId() const;
    quint64 nextSessionId() const;

    void initialize();
    void unInitialize();
    void reset();
    void resetStopFlag();
    void markStopFlag();
    bool isStopped() const;
    bool isSessionStopped(quint64 uiSessionId) const;

private:
    Q_DISABLE_COPY(NetworkRequestManagerPrivate);
    NetworkRequestManager *q_ptr;

private:
    static std::atomic<quint64> ms_uiRequestId;
    static std::atomic<quint64> ms_uiBatchId;
    static std::atomic<quint64> ms_uiSessionId;
    std::atomic<bool> m_bStopAllFlag;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    mutable QRecursiveMutex m_mutex;
#else
    mutable QMutex m_mutex;
#endif
    QThreadPool *m_pThreadPool;

    QHash<quint64, std::shared_ptr<NetworkRequestRunnable>> m_mapRunnable;
    // One-to-one. requestId <---> NetworkReply *
    QHash<quint64, std::shared_ptr<NetworkReply>> m_mapReply;
    // One-to-many. batchId <---> NetworkReply *
    QHash<quint64, std::shared_ptr<NetworkReply>> m_mapBatchReply;

    // session
    QMultiMap<quint64, quint64> m_mapSessionIdToRequestId;
    QSet<quint64> m_stoppedSessionIds;

    // (batchId <---> Total task count)
    QHash<quint64, size_t> m_mapBatchTotalSize;
    // (batchId <----> Task completion count)
    QHash<quint64, size_t> m_mapBatchFinishedSize;

    // (<batchId, <requestId, downloaded bytes>>)
    QHash<quint64, QHash<quint64, qint64>> m_mapBatchDCurrentBytes;
    // (batchId <---> Total download bytes)
    QHash<quint64, qint64> m_mapBatchDTotalBytes;
    // (<batchId, <requestId, uploaded bytes>>)
    QHash<quint64, QHash<quint64, qint64>> m_mapBatchUCurrentBytes;
    // (batchId <---> Total upload bytes)
    QHash<quint64, qint64> m_mapBatchUTotalBytes;
};
std::atomic<quint64> NetworkRequestManagerPrivate::ms_uiRequestId = 0;
std::atomic<quint64> NetworkRequestManagerPrivate::ms_uiBatchId = 0;
std::atomic<quint64> NetworkRequestManagerPrivate::ms_uiSessionId = 0;

NetworkRequestManagerPrivate::NetworkRequestManagerPrivate()
    : m_bStopAllFlag(false), m_pThreadPool(new QThreadPool), q_ptr(nullptr)
{
}

NetworkRequestManagerPrivate::~NetworkRequestManagerPrivate()
{
    qDebug() << "[QMultiThreadNetwork] Runnable size: " << m_mapRunnable.size();

    unInitialize();
    m_pThreadPool->deleteLater();
}

void NetworkRequestManagerPrivate::initialize()
{
    // Register meta types for signal/slot connections across threads
    qRegisterMetaType<QMap<QByteArray, QByteArray>>("QMap<QByteArray, QByteArray>");
    qRegisterMetaType<QSharedPointer<QtNetworkRequest::ResponseResult>>("QSharedPointer<QtNetworkRequest::ResponseResult>");

    int nIdeal = QThread::idealThreadCount();
    if (-1 != nIdeal)
    {
        m_pThreadPool->setMaxThreadCount(nIdeal);
    }
    else
    {
        m_pThreadPool->setMaxThreadCount(DEFAULT_MAX_THREAD_COUNT);
    }

    // To add something intialize...
}

void NetworkRequestManagerPrivate::unInitialize()
{
    stopAllRequest();
    reset();

    m_pThreadPool->clear();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!m_pThreadPool->waitForDone(1000))
    {
        qDebug() << "[QMultiThreadNetwork] ThreadPool waitForDone failed!";
    }
}

void NetworkRequestManagerPrivate::reset()
{
    QMutexLocker locker(&m_mutex);

    m_mapBatchTotalSize.clear();
    m_mapBatchFinishedSize.clear();
    m_mapBatchDCurrentBytes.clear();
    m_mapBatchDTotalBytes.clear();
    m_mapBatchUCurrentBytes.clear();
    m_mapBatchUTotalBytes.clear();

    m_mapRunnable.clear();
    m_mapReply.clear();
    m_mapBatchReply.clear();

    m_mapSessionIdToRequestId.clear();
    m_stoppedSessionIds.clear();
}

void NetworkRequestManagerPrivate::resetStopFlag()
{
    if (m_bStopAllFlag.load(std::memory_order_relaxed)) // Inter-thread synchronization reads are faster than writes
    {
        m_bStopAllFlag.store(false, std::memory_order_release);
    }
}

void NetworkRequestManagerPrivate::markStopFlag()
{
    if (!m_bStopAllFlag.load(std::memory_order_relaxed)) // Inter-thread synchronization reads are faster than writes
    {
        m_bStopAllFlag.store(true, std::memory_order_release);
    }
}

bool NetworkRequestManagerPrivate::isStopped() const
{
    return m_bStopAllFlag.load(std::memory_order_acquire);
}

bool NetworkRequestManagerPrivate::isSessionStopped(quint64 uiSessionId) const
{
    if (m_stoppedSessionIds.end() != m_stoppedSessionIds.find(uiSessionId))
    {
        return true;
    }
    return false;
}

void NetworkRequestManagerPrivate::stopRequest(quint64 uiTaskId)
{
    if (uiTaskId == 0)
        return;

    auto rsp = QSharedPointer<ResponseResult>::create();
    std::shared_ptr<NetworkReply> reply = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        reply = m_mapReply.take(uiTaskId);

        if (m_mapRunnable.contains(uiTaskId))
        {
            std::shared_ptr<NetworkRequestRunnable> r = m_mapRunnable.take(uiTaskId);
            if (r.get())
            {
                rsp->task = r->task();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
                if (!m_pThreadPool->tryTake(r.get()))
                {
                    r->quit();
                }
#else
                m_pThreadPool->cancel(r.get());
                r->quit();
#endif
                r.reset();
            }
        }
    }

    if (reply.get())
    {
        rsp->success = false;
        rsp->cancelled = true;
        rsp->body = QString("Operation canceled (id: %1)").arg(uiTaskId).toUtf8();
        rsp->task.endTime = QDateTime::currentDateTime();

        reply->replyResult(rsp, true);
    }
}

void NetworkRequestManagerPrivate::stopBatchRequests(quint64 uiBatchId)
{
    if (uiBatchId == 0)
        return;

    std::shared_ptr<NetworkReply> reply = nullptr;

    {
        QMutexLocker locker(&m_mutex);
        reply = m_mapBatchReply.take(uiBatchId);

        // qDebug() << "Runnable[Before]: " << m_mapRunnable.size();
        for (auto iter = m_mapRunnable.begin(); iter != m_mapRunnable.end();)
        {
            std::shared_ptr<NetworkRequestRunnable> r = iter.value();
            if (r.get() && r->batchId() == uiBatchId)
            {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
                if (!m_pThreadPool->tryTake(r.get()))
                {
                    r->quit();
                }
#else
                m_pThreadPool->cancel(r.get());
                r->quit();
#endif
                iter = m_mapRunnable.erase(iter);
                r.reset();
            }
            else
            {
                ++iter;
            }
        }
        // qDebug() << "Runnable[After]: " << m_mapRunnable.size();

        if (m_mapBatchTotalSize.contains(uiBatchId))
        {
            m_mapBatchTotalSize.remove(uiBatchId);
        }
        if (m_mapBatchFinishedSize.contains(uiBatchId))
        {
            m_mapBatchFinishedSize.remove(uiBatchId);
        }
        if (m_mapBatchDCurrentBytes.contains(uiBatchId))
        {
            m_mapBatchDCurrentBytes.remove(uiBatchId);
        }
        if (m_mapBatchDTotalBytes.contains(uiBatchId))
        {
            m_mapBatchDTotalBytes.remove(uiBatchId);
        }
        if (m_mapBatchUCurrentBytes.contains(uiBatchId))
        {
            m_mapBatchUCurrentBytes.remove(uiBatchId);
        }
        if (m_mapBatchUTotalBytes.contains(uiBatchId))
        {
            m_mapBatchUTotalBytes.remove(uiBatchId);
        }
    }

    if (reply.get())
    {
        auto rsp = QSharedPointer<ResponseResult>::create();
        rsp->task.batchId = uiBatchId;
        rsp->success = false;
        rsp->cancelled = true;
        rsp->body = QString("Operation canceled (Batch id: %1)").arg(uiBatchId).toUtf8();
        rsp->task.endTime = QDateTime::currentDateTime();

        reply->replyResult(rsp, true);
    }
}

void NetworkRequestManagerPrivate::stopSessionRequest(quint64 uiSessionId)
{
    if (uiSessionId == 0)
        return;

    QMutexLocker locker(&m_mutex);
    m_stoppedSessionIds.insert(uiSessionId);
    for (auto iter = m_mapRunnable.begin(); iter != m_mapRunnable.end();)
    {
        std::shared_ptr<NetworkRequestRunnable> r = iter.value();
        if (r.get() && r->sessionId() == uiSessionId)
        {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
            if (!m_pThreadPool->tryTake(r.get()))
            {
                r->quit();
            }
#else
            m_pThreadPool->cancel(r.get());
            r->quit();
#endif
            iter = m_mapRunnable.erase(iter);
            r.reset();
        }
        else
        {
            ++iter;
        }
    }

    QList<quint64> uiRequestIds = m_mapSessionIdToRequestId.values(uiSessionId);
    for (quint64 &uiRequestId : uiRequestIds)
    {
        if (m_mapReply.contains(uiRequestId))
        {
            m_mapReply.remove(uiRequestId);
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

        for (auto iter = m_mapRunnable.cbegin(); iter != m_mapRunnable.cend(); ++iter)
        {
            std::shared_ptr<NetworkRequestRunnable> r = iter.value();
            if (r.get())
            {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
                if (!m_pThreadPool->tryTake(r.get()))
                {
                    r->quit();
                }
#else
                m_pThreadPool->cancel(r.get());
                r->quit();
#endif
                r.reset();
            }
        }
        m_mapRunnable.clear();
    }
    reset();
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::postRequest(const QUrl &url, quint64 &uiId, quint64 uiSessionId)
{
    if (isValid(url))
    {
        uiId = nextRequestId();
        std::unique_ptr<TaskData> task = std::make_unique<TaskData>();
        task->id = uiId;
        task->sessionId = uiSessionId;
        std::shared_ptr<NetworkReply> pReply = std::make_shared<NetworkReply>(std::move(task));
        m_mapReply.insert(uiId, pReply);

        if (uiSessionId > 0)
        {
            m_mapSessionIdToRequestId.insert(uiSessionId, uiId);
        }

        return pReply;
    }
    return nullptr;
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &uiBatchId)
{
    if (tasks.empty())
        return nullptr;

    uiBatchId = nextBatchId();
    m_mapBatchTotalSize[uiBatchId] = tasks.size();

    std::unique_ptr<TaskData> task = std::make_unique<TaskData>();
    task->batchId = uiBatchId;
    std::shared_ptr<NetworkReply> pReply = std::make_shared<NetworkReply>(std::move(task));
    m_mapBatchReply.insert(uiBatchId, pReply);

    for (auto &context : tasks)
    {
        if (!context)
        {
            continue;
        }
        context->task.batchId = uiBatchId;
        context->task.id = nextRequestId();
        context->task.createTime = QDateTime::currentDateTime();

        Q_Q(NetworkRequestManager);
        q->startAsRunnable(std::move(context));
    }

    return pReply;
}

bool NetworkRequestManagerPrivate::sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool bBlockUserInteraction)
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
    if (bBlockUserInteraction)
        eventloop.exec(QEventLoop::ExcludeUserInputEvents);
    else
        eventloop.exec();
    return true;
}

quint64 NetworkRequestManagerPrivate::nextRequestId() const
{
    return ms_uiRequestId.fetch_add(1, std::memory_order_relaxed) + 1;
}

quint64 NetworkRequestManagerPrivate::nextBatchId() const
{
    return ms_uiBatchId.fetch_add(1, std::memory_order_relaxed) + 1;
}

quint64 NetworkRequestManagerPrivate::nextSessionId() const
{
    return ms_uiSessionId.fetch_add(1, std::memory_order_relaxed) + 1;
}

bool NetworkRequestManagerPrivate::startRunnable(std::shared_ptr<NetworkRequestRunnable> r, bool bAddToWaitQueueIfNotStart)
{
    if (r.get())
    {
        try
        {
            if (bAddToWaitQueueIfNotStart)
                m_pThreadPool->start(r.get());
            else
            {
                if (!m_pThreadPool->tryStart(r.get()))
                    return false;
            }
            {
                QMutexLocker locker(&m_mutex);
                m_mapRunnable.insert(r->requestId(), r);
            }
            return true;
        }
        catch (std::exception *e)
        {
            qCritical() << "[QMultiThreadNetwork] startRunnable() exception:" << QString::fromUtf8(e->what());
        }
        catch (...)
        {
            qCritical() << "[QMultiThreadNetwork] startRunnable() unknown exception";
        }
    }

    return false;
}

bool NetworkRequestManagerPrivate::setMaxThreadCount(int nMax)
{
    bool bRet = false;
    if (nMax >= 1 && nMax <= 100 && m_pThreadPool)
    {
        qDebug() << "[QMultiThreadNetwork] ThreadPool maxThreadCount: " << nMax;
        m_pThreadPool->setMaxThreadCount(nMax);
        bRet = true;
    }
    return bRet;
}

int NetworkRequestManagerPrivate::maxThreadCount() const
{
    if (m_pThreadPool)
    {
        return m_pThreadPool->maxThreadCount();
    }
    return -1;
}

bool NetworkRequestManagerPrivate::isThreadAvailable() const
{
    if (m_pThreadPool)
    {
        return (m_pThreadPool->activeThreadCount() < m_pThreadPool->maxThreadCount());
    }
    return false;
}

bool NetworkRequestManagerPrivate::isValid(const QUrl &url) const
{
    return (url.isValid());
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::getReply(quint64 uiRequestId, bool bRemove)
{
    QMutexLocker locker(&m_mutex);
    if (m_mapReply.contains(uiRequestId))
    {
        if (bRemove)
        {
            return m_mapReply.take(uiRequestId);
        }
        else
        {
            return m_mapReply.value(uiRequestId);
        }
    }
    qDebug() << QString("%1 failed! Id: ").arg(__FUNCTION__) << uiRequestId;
    return nullptr;
}

std::shared_ptr<NetworkReply> NetworkRequestManagerPrivate::getBatchReply(quint64 uiBatchId, bool bRemove)
{
    QMutexLocker locker(&m_mutex);
    if (m_mapBatchReply.contains(uiBatchId))
    {
        if (bRemove)
        {
            return m_mapBatchReply.take(uiBatchId);
        }
        else
        {
            return m_mapBatchReply.value(uiBatchId);
        }
    }
    return nullptr;
}

qint64 NetworkRequestManagerPrivate::updateBatchProgress(quint64 uiRequestId, quint64 uiBatchId, qint64 iBytes, qint64 iTotalBytes, bool bDownload)
{
    Q_UNUSED(iTotalBytes);
    // postEvent() calls are all in main thread, no need to lock

    // Bytes increased for this request task compared to last time (download/upload)
    quint64 uiIncreased = 0;
    quint64 uiTotalBytes = 0;
    if (iBytes == 0)
    {
        if (bDownload)
        {
            return m_mapBatchDTotalBytes[uiBatchId];
        }
        else
        {
            return m_mapBatchUTotalBytes[uiBatchId];
        }
    }

    if (bDownload)
    {
        const QHash<quint64, qint64> &mapReqId2Bytes = m_mapBatchDCurrentBytes.value(uiBatchId);
        if (mapReqId2Bytes.contains(uiRequestId))
        {
            qint64 curBytes = mapReqId2Bytes.value(uiRequestId);
            if (iBytes > curBytes)
            {
                uiIncreased = iBytes - curBytes;
            }
        }
        else
        {
            uiIncreased = iBytes;
        }
        m_mapBatchDCurrentBytes[uiBatchId][uiRequestId] = iBytes;

        uiTotalBytes = m_mapBatchDTotalBytes.value(uiBatchId) + uiIncreased;
        m_mapBatchDTotalBytes[uiBatchId] = uiTotalBytes;
    }
    else
    {
        const QHash<quint64, qint64> &mapReqId2Bytes = m_mapBatchUCurrentBytes.value(uiBatchId);
        if (mapReqId2Bytes.contains(uiRequestId))
        {
            qint64 curBytes = mapReqId2Bytes.value(uiRequestId);
            if (iBytes > curBytes)
            {
                uiIncreased = iBytes - curBytes;
            }
        }
        else
        {
            uiIncreased = iBytes;
        }
        m_mapBatchUCurrentBytes[uiBatchId][uiRequestId] = iBytes;

        uiTotalBytes = m_mapBatchUTotalBytes.value(uiBatchId) + uiIncreased;
        m_mapBatchUTotalBytes[uiBatchId] = uiTotalBytes;
    }

    return uiTotalBytes;
}

bool NetworkRequestManagerPrivate::releaseRequestThread(quint64 uiRequestId)
{
    QMutexLocker locker(&m_mutex);
    if (m_mapRunnable.contains(uiRequestId))
    {
        std::shared_ptr<NetworkRequestRunnable> r = m_mapRunnable.take(uiRequestId);
        if (r.get())
        {
            r->quit();
        }
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
std::atomic<bool> NetworkRequestManager::ms_bIntialized = false;
std::atomic<bool> NetworkRequestManager::ms_bUnIntializing = false;

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
    if (!ms_bIntialized)
    {
        NetworkRequestManager::globalInstance()->init();
        ms_bIntialized = true;
    }
}

void NetworkRequestManager::unInitialize()
{
    if (ms_bIntialized)
    {
        ms_bUnIntializing = true;
        NetworkRequestManager::globalInstance()->fini();
        ms_bIntialized = false;
        ms_bUnIntializing = false;
    }
}

bool NetworkRequestManager::isInitialized()
{
    return ms_bIntialized && !ms_bUnIntializing;
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

std::shared_ptr<NetworkReply> NetworkRequestManager::postBatchRequest(BatchRequestPtrTasks &&tasks, quint64 &uiBatchId)
{
    if (!NetworkRequestManager::isInitialized())
    {
        qDebug() << "[QMultiThreadNetwork] You must call NetworkRequestManager::initialize() before any request.";
        return nullptr;
    }

    Q_D(NetworkRequestManager);
    d->resetStopFlag();

    uiBatchId = 0;
    if (!tasks.empty())
    {
        std::shared_ptr<NetworkReply> pReply = d->postBatchRequest(std::move(tasks), uiBatchId);
        return pReply;
    }
    return nullptr;
}

bool NetworkRequestManager::sendRequest(std::unique_ptr<RequestContext> context, ResponseCallBack callback, bool bBlockUserInteraction)
{
    if (!NetworkRequestManager::isInitialized())
    {
        qDebug() << "[QMultiThreadNetwork] You must call NetworkRequestManager::initialize() before any request.";
        return false;
    }
    Q_D(NetworkRequestManager);
    return d->sendRequest(std::move(context), callback, bBlockUserInteraction);
}

void NetworkRequestManager::stopRequest(quint64 uiTaskId)
{
    Q_D(NetworkRequestManager);
    d->stopRequest(uiTaskId);
}

void NetworkRequestManager::stopBatchRequests(quint64 uiBatchId)
{
    Q_D(NetworkRequestManager);
    d->stopBatchRequests(uiBatchId);
}

void NetworkRequestManager::stopSessionRequest(quint64 uiSessionId)
{
    Q_D(NetworkRequestManager);
    d->stopSessionRequest(uiSessionId);
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

bool NetworkRequestManager::setMaxThreadCount(int iMax)
{
    Q_D(NetworkRequestManager);
    return d->setMaxThreadCount(iMax);
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
            updateProgress(evtProgress->uiId,
                           evtProgress->uiBatchId,
                           evtProgress->iBtyes,
                           evtProgress->iTotalBtyes,
                           evtProgress->bDownload);
        }
        return true;
    }

    return QObject::event(event);
}

void NetworkRequestManager::updateProgress(quint64 uiId, quint64 uiBatchId, qint64 iBytes, qint64 iTotalBytes, bool bDownload)
{
    Q_D(NetworkRequestManager);
    if (uiId == 0)
        return;

    // Find the reply for the single request
    std::shared_ptr<NetworkReply> singleReply = d->getReply(uiId, false); // Do not remove from map

    if (singleReply)
    {
        if (bDownload)
        {
            emit singleReply->downloadProgress(iBytes, iTotalBytes);
        }
        else
        {
            emit singleReply->uploadProgress(iBytes, iTotalBytes);
        }
    }

    if (uiBatchId > 0) // Batch request
    {
        // Find the reply for the batch
        std::shared_ptr<NetworkReply> batchReply = d->getBatchReply(uiBatchId, false); // Do not remove from map
        if (batchReply)
        {
            quint64 totalBatchBytes = d->updateBatchProgress(uiId, uiBatchId, iBytes, iTotalBytes, bDownload);
            if (bDownload)
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
        bool bDestroyed = true;
        auto batchId = rsp->task.batchId;
        if (batchId == 0)
        {
            pReply = d->getReply(rsp->task.id, bDestroyed);
        }
        else if (batchId > 0) // Batch task
        {
            size_t sizeFinished = 0;
            size_t sizeTotal = 0;
            {
                QMutexLocker locker(&d->m_mutex);
                sizeTotal = d->m_mapBatchTotalSize.value(rsp->task.batchId);
                if (sizeTotal > 0)
                {
                    sizeFinished = d->m_mapBatchFinishedSize.value(batchId);
                    d->m_mapBatchFinishedSize[batchId] = ++sizeFinished;

                    if (sizeFinished == sizeTotal)
                    {
                        d->m_mapBatchTotalSize.remove(batchId);
                        d->m_mapBatchFinishedSize.remove(batchId);
                    }
                }
            }

            if (rsp->success)
            {
                if (sizeFinished < sizeTotal) // Still have requests not completed
                {
                    bDestroyed = false;
                }
            }
            else // Batch task failed
            {
                if (!rsp->task.abortBatchOnFailed && (sizeFinished < sizeTotal))
                {
                    bDestroyed = false;
                }
            }
            pReply = d->getBatchReply(batchId, bDestroyed);
        }

        if (pReply.get())
        {
            pReply->replyResult(rsp, bDestroyed);
            if (batchId > 0 && bDestroyed)
            {
                qDebug() << QString("[QMultiThreadNetwork] Batch request finished! Id: %1").arg(batchId);
                emit batchRequestFinished(batchId, rsp->success);
            }
        }

        // 3. If batch task failed and bAbortBatchWhileOneFailed is specified, stop tasks in this batch
        if (batchId > 0 && !rsp->success && rsp->task.abortBatchOnFailed)
        {
            d->stopBatchRequests(batchId);
        }

        // 4. Release task thread to make it idle
        d->releaseRequestThread(rsp->task.id);
    }
    catch (std::exception *e)
    {
        qCritical() << "NetworkRequestManager::onResponse() exception:" << QString::fromUtf8(e->what());
    }
    catch (...)
    {
        qCritical() << "NetworkRequestManager::onResponse() unknown exception";
    }
}
