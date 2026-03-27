#ifndef DOWNLOADTASKMODEL_H
#define DOWNLOADTASKMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QTimer>
#include "downloadtask.h"

namespace QtNetworkRequest
{

class NetworkDownloadTaskModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum class Column : int
    {
        ColumnFileName = 0,
        ColumnFileSize = 1,
        ColumnDownloaded = 2,
        ColumnProgress = 3,
        ColumnSpeed = 4,
        ColumnTime = 5,
        ColumnState = 6,
        ColumnCount = 7
    };

    explicit NetworkDownloadTaskModel(QObject *parent = nullptr);
    ~NetworkDownloadTaskModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void addTask(const NetworkDownloadTask &task);
    void removeTask(const QString &id);
    void clearTasks();
    NetworkDownloadTask getTask(const QString &id) const;
    NetworkDownloadTask getTask(int row) const;
    QVector<NetworkDownloadTask> getAllTasks() const;
    void updateTask(const NetworkDownloadTask &task);
    void updateTaskProgress(const QString &id, qint64 downloadedBytes, qint64 totalBytes, qint64 speed);
    void updateTaskElapsedTime(const QString &id, qint64 elapsedMillis);
    void updateTaskTotalSpeed(const QString &id);
    void updateTaskState(const QString &id, NetworkDownloadTask::State state, const QString &error = QString());

    int getRunningTaskCount() const;
    qint64 getTotalSpeed() const;
    qint64 getTotalDownloaded() const;
    qint64 getTotalSize() const;

private:
    QVector<NetworkDownloadTask> m_tasks;
    QTimer m_updateTimer;

    int findTaskIndex(const QString &id) const;

private Q_SLOTS:
    void onTimerTimeout();
};

} // namespace QtNetworkRequest

#endif // DOWNLOADTASKMODEL_H
