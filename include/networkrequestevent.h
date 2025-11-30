#pragma once

#include <QEvent>
#include <QMap>
#include <QByteArray>
#include <QVariant>

namespace QtNetworkRequest
{
    ////////////////// Event ////////////////////////////////////////////////////
    namespace QEventRegister
    {
        template <typename T>
        int regiester(const T &eventName)
        {
            using UserEventMap = std::map<T, int>;
            static UserEventMap s_mapUserEvent;

            auto iter = s_mapUserEvent.find(eventName);
            if (iter != s_mapUserEvent.cend())
            {
                return iter->second;
            }

            int nEventType = QEvent::registerEventType();
            s_mapUserEvent[eventName] = nEventType;
            return nEventType;
        }
    };

    namespace NetworkEvent
    {
        const QEvent::Type WaitForIdleThread = (QEvent::Type)QEventRegister::regiester(QLatin1String("WaitForIdleThread"));
        const QEvent::Type ReplyResult = (QEvent::Type)QEventRegister::regiester(QLatin1String("ReplyResult"));
        const QEvent::Type NetworkProgress = (QEvent::Type)QEventRegister::regiester(QLatin1String("NetworkProgress"));
    }

    // 等待空闲线程事件
    class WaitForIdleThreadEvent : public QEvent
    {
    public:
        WaitForIdleThreadEvent() : QEvent(QEvent::Type(NetworkEvent::WaitForIdleThread)) {}
    };

    // 通知结果事件
    class ReplyResultEvent : public QEvent
    {
    public:
        ReplyResultEvent() : QEvent(QEvent::Type(NetworkEvent::ReplyResult)), bDestroyed(true) {}

        RequestTask request;
        bool bDestroyed;
    };

    // 下载/上传进度事件
    class NetworkProgressEvent : public QEvent
    {
    public:
        NetworkProgressEvent() : QEvent(QEvent::Type(NetworkEvent::NetworkProgress)), bDownload(true), uiId(0), uiBatchId(0), iBtyes(0), iTotalBtyes(0)
        {
        }

        bool bDownload;
        quint64 uiId;
        quint64 uiBatchId;
        qint64 iBtyes;
        qint64 iTotalBtyes;
    };
}
