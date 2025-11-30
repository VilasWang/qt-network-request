#include "networkreply.h"
#include <QDebug>
#include "networkrequestevent.h"

using namespace QtNetworkRequest;

NetworkReply::NetworkReply(std::unique_ptr<TaskData> task, QObject *parent /* = nullptr */)
    : QObject(parent), m_task(std::move(task))
{
}

NetworkReply::~NetworkReply()
{
    // Block all signals during destruction to prevent null receiver issues
    blockSignals(true);
}

bool NetworkReply::event(QEvent *event)
{
    if (event->type() == NetworkEvent::ReplyResult)
    {
        ReplyResultEvent *e = static_cast<ReplyResultEvent *>(event);
        if (nullptr != e)
        {
            replyResult(e->response, e->bDestroyed);
        }
        return true;
    }

    return QObject::event(event);
}

void NetworkReply::replyResult(QSharedPointer<QtNetworkRequest::ResponseResult> rsp, bool bDestroy)
{
    Q_UNUSED(bDestroy);
    
    // Check if the object is still valid before emitting signals
    // Use QSignalBlocker to prevent issues during destruction
    if (!signalsBlocked()) {
        emit requestFinished(rsp);
    }
}
