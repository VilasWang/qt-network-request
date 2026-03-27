#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include "downloadtask.h"
#include "../include/networkrequestmanager.h"
#include "../include/networkreply.h"

namespace QtNetworkRequest
{

class NetworkDownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkDownloadManager(QObject *parent = nullptr);
    ~NetworkDownloadManager();

    void setDownloadDirectory(const QString &dir);
    QString getDownloadDirectory() const;
    void setMaxThreads(int threads);
    int getMaxThreads() const;
    void setMaxConcurrentDownloads(int max);
    int getMaxConcurrentDownloads() const;

    void addDownloadTask(const NetworkDownloadTask &task);
    void addDownloadTaskForUIOnly(const NetworkDownloadTask &task);
    void startDownload(const QString &taskId);
    void pauseDownload(const QString &taskId);
    void cancelDownload(const QString &taskId);
    void removeDownload(const QString &taskId);

    bool isDownloading(const QString &taskId) const;
    NetworkDownloadTask::State getTaskState(const QString &taskId) const;

    void saveSettings();
    void loadSettings();
    void saveTasks();
    void loadTasks();

Q_SIGNALS:
    void taskAdded(const NetworkDownloadTask &task);
    void taskProgress(const QString &taskId, qint64 downloaded, qint64 total, qint64 speed);
    void taskElapsedTimeChanged(const QString &taskId, qint64 elapsedMillis);
    void taskStateChanged(const QString &taskId, NetworkDownloadTask::State state, const QString &error = QString());
    void taskCompleted(const QString &taskId, bool success);
    void downloadSpeedChanged(qint64 totalSpeed);
    void activeDownloadsChanged(int count);

private Q_SLOTS:
    void onDownloadProgress(const QString &taskId, qint64 bytesDownloaded, qint64 bytesTotal);
    void onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp);

private:
    struct DownloadInfo
    {
        NetworkDownloadTask task;
        std::shared_ptr<QtNetworkRequest::NetworkReply> reply;
        quint64 requestId;
        QTimer *speedTimer;
        qint64 lastDownloadedBytes;
        qint64 currentSpeed;
        QElapsedTimer downloadTimer;
        QDateTime lastTime;
        bool isActive;

        DownloadInfo() : reply(nullptr), requestId(0), speedTimer(nullptr),
                         lastDownloadedBytes(0), currentSpeed(0), isActive(false) {}
        ~DownloadInfo()
        {
            if (speedTimer)
            {
                if (speedTimer->isActive())
                    speedTimer->stop();
                speedTimer->deleteLater();
                speedTimer = nullptr;
            }
        }
    };

    QMap<QString, DownloadInfo> m_downloads;
    QString m_downloadDir;
    int m_maxThreads;
    int m_maxConcurrentDownloads;
    int m_activeDownloadCount;
    QSettings m_settings;
    int m_mIntervalMs{ 1000 };

    void startNextDownload();
    void updateDownloadSpeed(const QString &taskId);
    void cleanupDownload(const QString &taskId);
    std::unique_ptr<QtNetworkRequest::RequestContext> createRequestTask(const NetworkDownloadTask &task);
    QString generateUniqueFilePath(const QString &fileName) const;
};

} // namespace QtNetworkRequest

#endif // DOWNLOADMANAGER_H
