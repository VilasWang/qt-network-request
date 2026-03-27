#ifndef DOWNLOADTASK_H
#define DOWNLOADTASK_H

#include <QString>
#include <QUrl>
#include <QUuid>
#include <QMetaType>
#include <QFileInfo>

namespace QtNetworkRequest
{

struct NetworkDownloadTask
{
    QString id;
    QUrl url;
    QString fileName;
    qint64 totalBytes;
    qint64 downloadedBytes;
    int progress;
    qint64 speed;
    qint64 elapsedMillis;

    enum class State : int
    {
        Waiting = 0,
        Running = 1,
        Paused = 2,
        Completed = 3,
        Error = 4
    } state;

    QString errorMessage;
    QString savePath;

    NetworkDownloadTask()
        : totalBytes(-1), downloadedBytes(0), progress(0), speed(0), elapsedMillis(0), state(State::Waiting)
    {
    }

    NetworkDownloadTask(const QUrl &url, const QString &savePath = QString())
        : id(QUuid::createUuid().toString()), url(url), fileName(extractFileName(url)), totalBytes(-1), downloadedBytes(0), progress(0), speed(0), elapsedMillis(0), state(State::Waiting), savePath(savePath)
    {
    }

    static QString extractFileName(const QUrl &url)
    {
        QString path = url.path();
        if (path.isEmpty() || path == "/")
        {
            return "download";
        }

        QString fileName = QFileInfo(path).fileName();
        if (fileName.isEmpty())
        {
            return "download";
        }

        return fileName;
    }

    QString stateToString() const
    {
        switch (state)
        {
        case State::Waiting:
            return "Waiting";
        case State::Running:
            return "Downloading";
        case State::Paused:
            return "Paused";
        case State::Completed:
            return "Completed";
        case State::Error:
            return "Error";
        default:
            return "Unknown";
        }
    }

    static State stateFromString(const QString &stateStr)
    {
        if (stateStr == "Waiting")
            return State::Waiting;
        if (stateStr == "Downloading")
            return State::Running;
        if (stateStr == "Paused")
            return State::Paused;
        if (stateStr == "Completed")
            return State::Completed;
        if (stateStr == "Error")
            return State::Error;
        return State::Waiting;
    }

    QString formatFileSize(qint64 bytes) const
    {
        if (bytes < 0)
            return "--";
        if (bytes < 1024)
            return QString("%1 B").arg(bytes);
        if (bytes < 1024 * 1024)
            return QString("%1 KB").arg(bytes / 1024);
        if (bytes < 1024 * 1024 * 1024)
            return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }

    QString formatSpeed() const
    {
        return formatFileSize(speed) + "/s";
    }

    QString formatTime() const
    {
        if (elapsedMillis <= 0)
            return "--";

        qint64 seconds = elapsedMillis / 1000;
        if (seconds < 60)
            return QString("%1s").arg(seconds);

        qint64 minutes = seconds / 60;
        seconds = seconds % 60;
        if (minutes < 60)
            return QString("%1m %2s").arg(minutes).arg(seconds);

        qint64 hours = minutes / 60;
        minutes = minutes % 60;
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }

    bool isValid() const
    {
        return url.isValid() && !url.isEmpty();
    }
};

} // namespace QtNetworkRequest

Q_DECLARE_METATYPE(QtNetworkRequest::NetworkDownloadTask)

#endif // DOWNLOADTASK_H
