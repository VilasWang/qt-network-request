#include "networkrequestrunnable.h"
#include <QDebug>
#include <QEventLoop>
#include <QCoreApplication>
#include "networkrequest.h"
#include "networkrequestmanager.h"
#include <QMutexLocker>

using namespace QtNetworkRequest;

NetworkRequestRunnable::NetworkRequestRunnable(std::unique_ptr<RequestContext> request, QObject* parent)
    : QObject(parent), m_context(std::move(request)), m_bAbort(false)
{
    setAutoDelete(false);
    if (m_context)
    {
        m_task = m_context->task;
    }
}

NetworkRequestRunnable::~NetworkRequestRunnable()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_context)
        {
            m_context.reset();
        }
    }
}

void NetworkRequestRunnable::run()
{
    QDateTime startTime = QDateTime::currentDateTime();
    RequestType type = RequestType::Unknown;
    {
        QMutexLocker locker(&m_mutex);
        if (m_context)
        {
            type = m_context->type;
        }
    }
    std::unique_ptr<NetworkRequest> pRequest = nullptr;
    QEventLoop loop;

    try
    {
        connect(this, &NetworkRequestRunnable::exitLoop, &loop, [&loop]() { loop.quit(); }, Qt::QueuedConnection);

        {
            QMutexLocker locker(&m_mutex);
            if (m_context)
            {
                pRequest = std::move(NetworkRequestFactory::create(std::move(m_context)));
            }
        }
        if (pRequest.get())
        {
            m_connect = connect(pRequest.get(), &NetworkRequest::response, this,
                                [=](QSharedPointer<QtNetworkRequest::ResponseResult> rsp) {
                rsp->task.startTime = startTime;
                rsp->task.endTime = QDateTime::currentDateTime();
                rsp->cancelled = m_bAbort;
                emit response(rsp);
            });
            pRequest->start();
        }
        else
        {
            auto rsp = QSharedPointer<ResponseResult>::create();
            rsp->task.startTime = startTime;
            rsp->task.endTime = QDateTime::currentDateTime();
            rsp->success = false;
            rsp->errorMessage = QString("[QMultiThreadNetwork] Configuration error: Unsupported request type (%1)").arg((qint32)type);
            emit response(rsp);
        }
        loop.exec();
    }
    catch (std::exception* e)
    {
        qCritical() << "[QMultiThreadNetwork] NetworkRequestRunnable::run() exception:" << QString::fromUtf8(e->what());
    }
    catch (...)
    {
        qCritical() << "[QMultiThreadNetwork] NetworkRequestRunnable::run() unknown exception";
    }

    if (pRequest.get())
    {
        pRequest->abort();
        pRequest.reset();
    }
}

quint64 NetworkRequestRunnable::requestId() const
{
    return m_task.id;
}

quint64 NetworkRequestRunnable::batchId() const
{
    return m_task.batchId;
}

quint64 NetworkRequestRunnable::sessionId() const
{
    return m_task.sessionId;
}

void NetworkRequestRunnable::quit()
{
    m_bAbort = true;
    this->disconnect(m_connect);
    emit exitLoop();
}