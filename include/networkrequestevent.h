/*
@Brief:			Qt multi-threaded network request module

The Qt multi-threaded network request module is a wrapper of Qt Network module, and combine with thread-pool to realize multi-threaded networking.
- Multi-task concurrent(Each request task is executed in different threads).
- Both single request and batch request mode are supported.
- Large file multi-thread downloading supported. (The thread here refers to the download channel. Download speed is faster.)
- HTTP(S)/FTP protocol supported.
- Multiple request methods supported. (GET/POST/PUT/DELETE/HEAD)
- Asynchronous API.
- Thread-safe.

Note: You must call NetworkRequestManager::initialize() before use, and call NetworkRequestManager::unInitialize() before application quit.
That must be called in the main thread.
*/

#ifndef NETWORKEVENT_H
#define NETWORKEVENT_H
#pragma once

#include <QEvent>
#include <QMap>
#include <QByteArray>
#include <QVariant>
#include <QSharedPointer>
#include "responseresult.h"

namespace QtNetworkRequest
{
    ////////////////// Event ////////////////////////////////////////////////////
    namespace QEventRegister
    {
        template <typename T>
        int registerEvent(const T &eventName)
        {
            using UserEventMap = std::map<T, int>;
            static UserEventMap s_mapUserEvent;

            auto iter = s_mapUserEvent.find(eventName);
            if (iter != s_mapUserEvent.cend())
            {
                return iter->second;
            }

            int eventType = QEvent::registerEventType();
            s_mapUserEvent[eventName] = eventType;
            return eventType;
        }

        // Keep old spelling for backward compatibility
        template <typename T>
        inline int regiester(const T &eventName) { return registerEvent(eventName); }
    };

    namespace NetworkEvent
    {
        const QEvent::Type WaitForIdleThread = (QEvent::Type)QEventRegister::registerEvent(QString("WaitForIdleThread"));
        const QEvent::Type ReplyResult = (QEvent::Type)QEventRegister::registerEvent(QString("ReplyResult"));
        const QEvent::Type NetworkProgress = (QEvent::Type)QEventRegister::registerEvent(QString("NetworkProgress"));
    }

    // Wait for idle thread event
    class WaitForIdleThreadEvent : public QEvent
    {
    public:
        WaitForIdleThreadEvent() : QEvent(QEvent::Type(NetworkEvent::WaitForIdleThread)) {}
    };

    // Notify result event
    class ReplyResultEvent : public QEvent
    {
    public:
        ReplyResultEvent() : QEvent(QEvent::Type(NetworkEvent::ReplyResult)), isDestroyed(true) {}

        QSharedPointer<ResponseResult> response;
        bool isDestroyed;
    };

    // Download/Upload progress event
    class NetworkProgressEvent : public QEvent
    {
    public:
        NetworkProgressEvent() : QEvent(QEvent::Type(NetworkEvent::NetworkProgress)), isDownload(true), requestId(0), batchId(0), transferredBytes(0), totalBytes(0)
        {
        }

        bool isDownload;
        quint64 requestId;
        quint64 batchId;
        qint64 transferredBytes;
        qint64 totalBytes;
    };
}

#endif /// NETWORKEVENT_H
