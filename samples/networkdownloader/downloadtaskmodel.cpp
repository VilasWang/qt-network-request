#include "downloadtaskmodel.h"
#include <QIcon>
#include <QTimer>

QtNetworkRequest::NetworkDownloadTaskModel::NetworkDownloadTaskModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_updateTimer.setInterval(500); // Update every 500ms
    connect(&m_updateTimer, &QTimer::timeout, this, &QtNetworkRequest::NetworkDownloadTaskModel::onTimerTimeout);
    m_updateTimer.start();
}

QtNetworkRequest::NetworkDownloadTaskModel::~NetworkDownloadTaskModel()
{
}

int QtNetworkRequest::NetworkDownloadTaskModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tasks.size();
}

int QtNetworkRequest::NetworkDownloadTaskModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(Column::ColumnCount);
}

QVariant QtNetworkRequest::NetworkDownloadTaskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tasks.size())
        return QVariant();

    const QtNetworkRequest::NetworkDownloadTask &task = m_tasks[index.row()];
    
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Column::ColumnFileName:
            return task.fileName;
        case Column::ColumnFileSize:
            return task.formatFileSize(task.totalBytes);
        case Column::ColumnDownloaded:
            return task.formatFileSize(task.downloadedBytes);
        case Column::ColumnProgress:
            return QString("%1%").arg(task.progress);
        case Column::ColumnSpeed:
            return task.formatSpeed();
        case Column::ColumnTime:
            return task.formatTime();
        case Column::ColumnState:
            return task.stateToString();
        }
        break;
        
    case Qt::TextAlignmentRole:
        if (index.column() == static_cast<int>(Column::ColumnProgress) || index.column() == static_cast<int>(Column::ColumnSpeed))
            return Qt::AlignCenter;
        break;
        
    case Qt::DecorationRole:
        if (index.column() == static_cast<int>(Column::ColumnState)) {
            switch (task.state) {
            case QtNetworkRequest::NetworkDownloadTask::State::Waiting:
                return QIcon::fromTheme("media-playback-pause");
            case QtNetworkRequest::NetworkDownloadTask::State::Running:
                return QIcon::fromTheme("media-playback-start");
            case QtNetworkRequest::NetworkDownloadTask::State::Paused:
                return QIcon::fromTheme("media-playback-pause");
            case QtNetworkRequest::NetworkDownloadTask::State::Completed:
                return QIcon::fromTheme("dialog-ok");
            case QtNetworkRequest::NetworkDownloadTask::State::Error:
                return QIcon::fromTheme("dialog-error");
            }
        }
        break;
        
    case Qt::ToolTipRole:
        if (index.column() == static_cast<int>(Column::ColumnState) && task.state == QtNetworkRequest::NetworkDownloadTask::State::Error) {
            return task.errorMessage;
        }
        break;
        
    case Qt::ForegroundRole:
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Error)
            return QColor(Qt::red);
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Completed)
            return QColor(Qt::darkGreen);
        break;
    }
    
    return QVariant();
}

QVariant QtNetworkRequest::NetworkDownloadTaskModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    
    switch (section) {
    case Column::ColumnFileName:
        return "File Name";
    case Column::ColumnFileSize:
        return "Size";
    case Column::ColumnDownloaded:
        return "Downloaded";
    case Column::ColumnProgress:
        return "Progress";
    case Column::ColumnSpeed:
        return "Speed";
    case Column::ColumnTime:
        return "Time";
    case Column::ColumnState:
        return "Status";
    }
    
    return QVariant();
}

Qt::ItemFlags QtNetworkRequest::NetworkDownloadTaskModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void QtNetworkRequest::NetworkDownloadTaskModel::addTask(const QtNetworkRequest::NetworkDownloadTask &task)
{
    if (!task.isValid())
        return;
    
    beginInsertRows(QModelIndex(), m_tasks.size(), m_tasks.size());
    m_tasks.append(task);
    endInsertRows();
}

void QtNetworkRequest::NetworkDownloadTaskModel::removeTask(const QString &id)
{
    int index = findTaskIndex(id);
    if (index >= 0) {
        beginRemoveRows(QModelIndex(), index, index);
        m_tasks.removeAt(index);
        endRemoveRows();
    }
}

void QtNetworkRequest::NetworkDownloadTaskModel::clearTasks()
{
    beginResetModel();
    m_tasks.clear();
    endResetModel();
}

QtNetworkRequest::NetworkDownloadTask QtNetworkRequest::NetworkDownloadTaskModel::getTask(const QString &id) const
{
    int index = findTaskIndex(id);
    if (index >= 0) {
        return m_tasks[index];
    }
    return QtNetworkRequest::NetworkDownloadTask();
}

QtNetworkRequest::NetworkDownloadTask QtNetworkRequest::NetworkDownloadTaskModel::getTask(int row) const
{
    if (row >= 0 && row < m_tasks.size()) {
        return m_tasks[row];
    }
    return QtNetworkRequest::NetworkDownloadTask();
}

QVector<QtNetworkRequest::NetworkDownloadTask> QtNetworkRequest::NetworkDownloadTaskModel::getAllTasks() const
{
    return m_tasks;
}

void QtNetworkRequest::NetworkDownloadTaskModel::updateTask(const QtNetworkRequest::NetworkDownloadTask &task)
{
    int index = findTaskIndex(task.id);
    if (index >= 0) {
        m_tasks[index] = task;
        emit dataChanged(createIndex(index, 0), createIndex(index, static_cast<int>(Column::ColumnCount) - 1));
    }
}

void QtNetworkRequest::NetworkDownloadTaskModel::updateTaskProgress(const QString &id, qint64 downloadedBytes, qint64 totalBytes, qint64 speed)
{
    int index = findTaskIndex(id);
    if (index >= 0) {
        QtNetworkRequest::NetworkDownloadTask &task = m_tasks[index];
        task.downloadedBytes = downloadedBytes;
        task.totalBytes = totalBytes;
        task.speed = speed;
        
        if (totalBytes > 0) {
            task.progress = static_cast<int>((downloadedBytes * 100) / totalBytes);
        }
        
        emit dataChanged(createIndex(index, static_cast<int>(Column::ColumnDownloaded)), createIndex(index, static_cast<int>(Column::ColumnSpeed)));
    }
}

void QtNetworkRequest::NetworkDownloadTaskModel::updateTaskState(const QString &id, QtNetworkRequest::NetworkDownloadTask::State state, const QString &error)
{
    int index = findTaskIndex(id);
    if (index >= 0) {
        QtNetworkRequest::NetworkDownloadTask &task = m_tasks[index];
        task.state = state;
        task.errorMessage = error;
        
        if (state == QtNetworkRequest::NetworkDownloadTask::State::Completed) {
            task.progress = 100;
            task.speed = 0;
        } else if (state == QtNetworkRequest::NetworkDownloadTask::State::Error) {
            task.speed = 0;
        }
        
        emit dataChanged(createIndex(index, static_cast<int>(Column::ColumnProgress)), createIndex(index, static_cast<int>(Column::ColumnTime)));
    }
}

void QtNetworkRequest::NetworkDownloadTaskModel::updateTaskElapsedTime(const QString &id, qint64 elapsedMillis)
{
    int index = findTaskIndex(id);
    if (index >= 0) {
        QtNetworkRequest::NetworkDownloadTask &task = m_tasks[index];
        task.elapsedMillis = elapsedMillis;
        
        emit dataChanged(createIndex(index, static_cast<int>(Column::ColumnTime)), createIndex(index, static_cast<int>(Column::ColumnTime)));
    }
}

void QtNetworkRequest::NetworkDownloadTaskModel::updateTaskTotalSpeed(const QString& id)
{
	int index = findTaskIndex(id);
	if (index >= 0) {
		QtNetworkRequest::NetworkDownloadTask& task = m_tasks[index];
        if (task.elapsedMillis > 0)
            task.speed = task.totalBytes / task.elapsedMillis;
        else
            task.speed = 0;

		emit dataChanged(createIndex(index, static_cast<int>(Column::ColumnTime)), createIndex(index, static_cast<int>(Column::ColumnTime)));
	}
}

int QtNetworkRequest::NetworkDownloadTaskModel::getRunningTaskCount() const
{
    int count = 0;
    for (const QtNetworkRequest::NetworkDownloadTask &task : m_tasks) {
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running) {
            count++;
        }
    }
    return count;
}

qint64 QtNetworkRequest::NetworkDownloadTaskModel::getTotalSpeed() const
{
    qint64 totalSpeed = 0;
    for (const QtNetworkRequest::NetworkDownloadTask &task : m_tasks) {
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running) {
            totalSpeed += task.speed;
        }
    }
    return totalSpeed;
}

qint64 QtNetworkRequest::NetworkDownloadTaskModel::getTotalDownloaded() const
{
    qint64 total = 0;
    for (const QtNetworkRequest::NetworkDownloadTask &task : m_tasks) {
        total += task.downloadedBytes;
    }
    return total;
}

qint64 QtNetworkRequest::NetworkDownloadTaskModel::getTotalSize() const
{
    qint64 total = 0;
    for (const QtNetworkRequest::NetworkDownloadTask &task : m_tasks) {
        if (task.totalBytes > 0) {
            total += task.totalBytes;
        }
    }
    return total;
}

int QtNetworkRequest::NetworkDownloadTaskModel::findTaskIndex(const QString &id) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].id == id) {
            return i;
        }
    }
    return -1;
}

void QtNetworkRequest::NetworkDownloadTaskModel::onTimerTimeout()
{
    // Emit data changed for running tasks to update speed and time display
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].state == QtNetworkRequest::NetworkDownloadTask::State::Running) {
            emit dataChanged(createIndex(i, static_cast<int>(Column::ColumnSpeed)), createIndex(i, static_cast<int>(Column::ColumnTime)));
        }
    }
}