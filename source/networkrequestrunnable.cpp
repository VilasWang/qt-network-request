#include "networkrequestrunnable.h"
#include <QDebug>
#include <QEventLoop>
#include <QCoreApplication>
#include <QThread>
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
        m_nPriority = m_context->behavior.priority;
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
    int totalMs = 0;
    quint64 taskId = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_context)
        {
            type = m_context->type;
            totalMs = m_context->behavior.totalTimeoutMs;
            taskId = m_context->task.id;
        }
    }

    // Reset state for this run
    m_responseSent.store(false);
    m_bRunning.store(true);

    std::unique_ptr<NetworkRequest> pRequest = nullptr;
    QEventLoop loop;

    try
    {
        connect(this, &NetworkRequestRunnable::exitLoop, &loop, &QEventLoop::quit);

        // --- Layer1: Total request timeout ---
        if (totalMs > 0)
        {
            QTimer::singleShot(totalMs, this, [this, totalMs, taskId]() {
                bool expected = false;
                if (!m_responseSent.compare_exchange_strong(expected, true))
                    return;

                qWarning() << "[QMultiThreadNetwork] Request total timeout, taskId:" << taskId;

                m_bAbort = true;
                if (m_connect)
                    disconnect(m_connect);

                auto rsp = QSharedPointer<ResponseResult>::create();
                rsp->task.id = taskId;
                rsp->error.category = ErrorCategory::Timeout;
                rsp->error.code = ErrorCode::TimeoutTotal;
                rsp->error.nativeCode = static_cast<int>(QNetworkReply::TimeoutError);
                rsp->error.message = QStringLiteral("Request total timeout (%1ms)").arg(totalMs);
                emit response(rsp);

                emit exitLoop();
            });
        }

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
                                [this, startTime](QSharedPointer<QtNetworkRequest::ResponseResult> rsp) {
                // CAS: only proceed if timeout path hasn't already sent
                bool expected = false;
                if (!m_responseSent.compare_exchange_strong(expected, true))
                {
                    return; // timeout path already sent the response
                }

                rsp->task.startTime = startTime;
                rsp->task.endTime = QDateTime::currentDateTime();
                // If cancelled and the request didn't already classify the error
                // as cancellation/timeout, mark it as a user cancellation.
                if (m_bAbort && rsp->error.category != ErrorCategory::Cancelled
                              && rsp->error.category != ErrorCategory::Timeout)
                {
                    rsp->error.category = ErrorCategory::Cancelled;
                    rsp->error.code = ErrorCode::OperationCancelled;
                    if (rsp->error.message.isEmpty())
                        rsp->error.message = QStringLiteral("Operation canceled");
                }
                emit response(rsp);
            });
            pRequest->start();
        }
        else
        {
            bool expected = false;
            if (m_responseSent.compare_exchange_strong(expected, true))
            {
                auto rsp = QSharedPointer<ResponseResult>::create();
                rsp->task.startTime = startTime;
                rsp->task.endTime = QDateTime::currentDateTime();
                rsp->error.category = ErrorCategory::Configuration;
                rsp->error.code = ErrorCode::UnsupportedRequestType;
                rsp->error.message = QString("[QMultiThreadNetwork] Configuration error: Unsupported request type (%1)").arg((qint32)type);
                emit response(rsp);
            }
        }
        loop.exec();
    }
    catch (const std::exception &e)
    {
        qCritical() << "[QMultiThreadNetwork] NetworkRequestRunnable::run() exception:" << QString::fromUtf8(e.what());
    }
    catch (...)
    {
        qCritical() << "[QMultiThreadNetwork] NetworkRequestRunnable::run() unknown exception";
    }

    // --- Cleanup ---

    // Abort the request if it hasn't already sent a response (including timeout)
    if (pRequest && !m_responseSent.load())
    {
        pRequest->abort();
    }

    // Defensive: ensure at least one response is always sent
    if (!m_responseSent.load())
    {
        auto rsp = QSharedPointer<ResponseResult>::create();
        if (m_bAbort)
        {
            rsp->error.category = ErrorCategory::Cancelled;
            rsp->error.code = ErrorCode::OperationCancelled;
            rsp->error.message = QStringLiteral("Operation canceled");
        }
        else
        {
            rsp->error.category = ErrorCategory::Unknown;
            rsp->error.code = ErrorCode::TerminatedWithoutResponse;
            rsp->error.message = QStringLiteral("Request terminated without response");
        }
        emit response(rsp);
    }
    pRequest.reset();
    m_bRunning.store(false);

    // Signal completion LAST — after every access to this object on the worker
    // thread is done. Delivered to the manager on the main thread (queued), so
    // the runnable is destroyed only after run() has fully returned. This is
    // what makes cancelling a running request safe (no use-after-free).
    emit finished(m_task.id);
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
    // Only emit exitLoop if run() is still active (event loop is running).
    // After timeout, run() already returned and the loop is destroyed.
    if (m_bRunning.load())
    {
        emit exitLoop();
    }
}