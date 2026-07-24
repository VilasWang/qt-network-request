#include "downloadmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QDebug>

QtNetworkRequest::NetworkDownloadManager::NetworkDownloadManager(QObject *parent)
    : QObject(parent), m_settings("QtDownloader", "Downloader"), m_maxThreads(4), m_maxConcurrentDownloads(3), m_activeDownloadCount(0)
{
    // Initialize NetworkRequestManager
    QtNetworkRequest::NetworkRequestManager::initialize();

    // Load settings
    loadSettings();

    // Set default download directory if not set
    if (m_downloadDir.isEmpty())
    {
        m_downloadDir = QDir::homePath() + "/Downloads";
        QDir().mkpath(m_downloadDir);
    }
}

QtNetworkRequest::NetworkDownloadManager::~NetworkDownloadManager()
{
    // Cancel all active downloads and clean up timers
    for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it)
    {
        if (it->reply)
        {
            it->reply = nullptr;
        }
        if (it->speedTimer)
        {
            if (it->speedTimer->isActive())
                it->speedTimer->stop();
            it->speedTimer->deleteLater();
            it->speedTimer = nullptr;
        }
    }

    // Clear all downloads
    m_downloads.clear();

    // Uninitialize NetworkRequestManager
    QtNetworkRequest::NetworkRequestManager::unInitialize();
}

void QtNetworkRequest::NetworkDownloadManager::setDownloadDirectory(const QString &dir)
{
    m_downloadDir = dir;
    QDir().mkpath(m_downloadDir);
    saveSettings();
}

QString QtNetworkRequest::NetworkDownloadManager::getDownloadDirectory() const
{
    return m_downloadDir;
}

void QtNetworkRequest::NetworkDownloadManager::setMaxThreads(int threads)
{
    m_maxThreads = qBound(1, threads, 64);
    saveSettings();
}

int QtNetworkRequest::NetworkDownloadManager::getMaxThreads() const
{
    return m_maxThreads;
}

void QtNetworkRequest::NetworkDownloadManager::setMaxConcurrentDownloads(int max)
{
    m_maxConcurrentDownloads = qBound(1, max, 20);
    saveSettings();
}

int QtNetworkRequest::NetworkDownloadManager::getMaxConcurrentDownloads() const
{
    return m_maxConcurrentDownloads;
}

void QtNetworkRequest::NetworkDownloadManager::addDownloadTask(const QtNetworkRequest::NetworkDownloadTask &task)
{
    if (!task.isValid())
        return;

    DownloadInfo info;
    info.task = task;
    info.reply = nullptr;
    info.requestId = 0;
    info.speedTimer = nullptr;
    info.lastDownloadedBytes = 0;
    info.currentSpeed = 0;
    info.lastSampleElapsed = 0;
    info.smoothSpeed = 0.0;
    info.isActive = false;

    m_downloads[task.id] = std::move(info);
    emit taskAdded(task);
    saveTasks(); // Save tasks when adding new ones

    // Start download if we have available slots
    if (m_activeDownloadCount < m_maxConcurrentDownloads)
    {
        startDownload(task.id);
    }
}

void QtNetworkRequest::NetworkDownloadManager::addDownloadTaskForUIOnly(const QtNetworkRequest::NetworkDownloadTask &task)
{
    if (!task.isValid())
        return;

        
    DownloadInfo info;
    info.task = task;
    info.reply = nullptr;
    info.requestId = 0;
    info.speedTimer = nullptr; // Don't create timer for UI-only tasks
    info.lastDownloadedBytes = 0;
    info.currentSpeed = 0;
    info.isActive = false;

    m_downloads[task.id] = std::move(info);
    emit taskAdded(task);
    // saveTasks(); // Save tasks when adding new ones

    // Don't auto-start download - user will manually start it
}

void QtNetworkRequest::NetworkDownloadManager::startDownload(const QString &taskId)
{
    if (!m_downloads.contains(taskId))
        return;

    DownloadInfo &info = m_downloads[taskId];

    if (info.isActive || info.task.state == QtNetworkRequest::NetworkDownloadTask::State::Completed)
        return;

    // Check if we can start a new download
    if (m_activeDownloadCount >= m_maxConcurrentDownloads)
        return;

    QtNetworkRequest::NetworkRequestManager *manager = QtNetworkRequest::NetworkRequestManager::globalInstance();
    info.reply = manager->postRequest(createRequestTask(info.task));

    if (info.reply)
    {
        connect(info.reply.get(), &QtNetworkRequest::NetworkReply::requestFinished, this, &NetworkDownloadManager::onResponse);

        // Connect to the specific reply for progress updates
        connect(info.reply.get(), &QtNetworkRequest::NetworkReply::downloadProgress, this, [this, taskId](qint64 bytesDownloaded, qint64 bytesTotal) {
            onDownloadProgress(taskId, bytesDownloaded, bytesTotal);
        });

        auto task = info.reply->task();

        info.isActive = true;
        info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Running;
        info.requestId = task->id;
        info.lastSampleElapsed = 0;
        info.smoothSpeed = 0.0;
        info.downloadTimer.start();
        m_activeDownloadCount++;

        // Create new timer if needed (for UI-only tasks that don't have one)
        if (!info.speedTimer)
        {
            info.speedTimer = new QTimer();
        }

        // Setup speed calculation timer using direct connection
        connect(info.speedTimer, &QTimer::timeout, this, [this, taskId]()
                {
            if (m_downloads.contains(taskId)) {
                updateDownloadSpeed(taskId);
            } });
        info.speedTimer->start(m_mIntervalMs); // Update speed every second

        emit taskStateChanged(taskId, QtNetworkRequest::NetworkDownloadTask::State::Running);
        emit activeDownloadsChanged(m_activeDownloadCount);
    }
    else
    {
        info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Error;
        info.task.errorMessage = "Failed to create download request";
        emit taskStateChanged(taskId, QtNetworkRequest::NetworkDownloadTask::State::Error, info.task.errorMessage);
    }
}

void QtNetworkRequest::NetworkDownloadManager::pauseDownload(const QString &taskId)
{
    if (!m_downloads.contains(taskId))
        return;

    DownloadInfo &info = m_downloads[taskId];

    if (!info.isActive)
        return;

    if (info.reply)
    {
        QtNetworkRequest::NetworkRequestManager::globalInstance()->stopRequest(info.requestId);
        info.reply = nullptr;
    }

    info.isActive = false;
    info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Paused;
    if (info.speedTimer)
    {
        if (info.speedTimer->isActive())
            info.speedTimer->stop();
        info.speedTimer->deleteLater();
        info.speedTimer = nullptr;
    }
    m_activeDownloadCount--;

    emit taskStateChanged(taskId, QtNetworkRequest::NetworkDownloadTask::State::Paused);
    emit activeDownloadsChanged(m_activeDownloadCount);

    // Start next download if available
    startNextDownload();
}

void QtNetworkRequest::NetworkDownloadManager::cancelDownload(const QString &taskId)
{
    if (!m_downloads.contains(taskId))
        return;

    DownloadInfo &info = m_downloads[taskId];

    if (info.isActive)
    {
        if (info.reply)
        {
            QtNetworkRequest::NetworkRequestManager::globalInstance()->stopRequest(info.requestId);
            info.reply = nullptr;
        }

        info.isActive = false;
        if (info.speedTimer)
        {
			if (info.speedTimer->isActive())
				info.speedTimer->stop();
            info.speedTimer->deleteLater();
            info.speedTimer = nullptr;
        }
        m_activeDownloadCount--;
        emit activeDownloadsChanged(m_activeDownloadCount);
    }

    info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Waiting;
    info.task.downloadedBytes = 0;
    info.task.progress = 0;
    info.task.speed = 0;

    emit taskStateChanged(taskId, QtNetworkRequest::NetworkDownloadTask::State::Waiting);
    emit taskProgress(taskId, 0, -1, 0);

    // Start next download if available
    startNextDownload();
}

void QtNetworkRequest::NetworkDownloadManager::removeDownload(const QString &taskId)
{
    if (!m_downloads.contains(taskId))
        return;

    DownloadInfo info = m_downloads[taskId]; // Make a copy to work with

    // Clean up resources for both active and completed tasks
    if (info.reply)
    {
        QtNetworkRequest::NetworkRequestManager::globalInstance()->stopRequest(info.requestId);
        info.reply = nullptr;
    }

    if (info.speedTimer)
    {
		if (info.speedTimer->isActive())
			info.speedTimer->stop();
        info.speedTimer->deleteLater();
        info.speedTimer = nullptr;
    }

    if (info.isActive)
    {
        info.isActive = false;
        m_activeDownloadCount--;
        emit activeDownloadsChanged(m_activeDownloadCount);
    }

    m_downloads.remove(taskId);
    saveTasks(); // Save tasks when removing

    // Start next download if available
    startNextDownload();
}

bool QtNetworkRequest::NetworkDownloadManager::isDownloading(const QString &taskId) const
{
    if (m_downloads.contains(taskId))
    {
        return m_downloads[taskId].isActive;
    }
    return false;
}

QtNetworkRequest::NetworkDownloadTask::State QtNetworkRequest::NetworkDownloadManager::getTaskState(const QString &taskId) const
{
    if (m_downloads.contains(taskId))
    {
        return m_downloads[taskId].task.state;
    }
    return QtNetworkRequest::NetworkDownloadTask::State::Waiting;
}

QtNetworkRequest::NetworkDownloadTask QtNetworkRequest::NetworkDownloadManager::getDownloadTask(const QString &taskId) const
{
    auto it = m_downloads.find(taskId);
    if (it != m_downloads.end())
        return it->task;
    return QtNetworkRequest::NetworkDownloadTask();
}

void QtNetworkRequest::NetworkDownloadManager::saveSettings()
{
    m_settings.setValue("DownloadDirectory", m_downloadDir);
    m_settings.setValue("MaxThreads", m_maxThreads);
    m_settings.setValue("MaxConcurrentDownloads", m_maxConcurrentDownloads);
}

void QtNetworkRequest::NetworkDownloadManager::loadSettings()
{
    m_downloadDir = m_settings.value("DownloadDirectory", QString()).toString();
    m_maxThreads = m_settings.value("MaxThreads", 16).toInt();
    m_maxConcurrentDownloads = m_settings.value("MaxConcurrentDownloads", 5).toInt();
    loadTasks();
}

void QtNetworkRequest::NetworkDownloadManager::saveTasks()
{
    m_settings.beginGroup("Tasks");
    m_settings.remove(""); // Clear existing tasks

    int index = 0;
    for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it)
    {
        const QtNetworkRequest::NetworkDownloadTask &task = it->task;

        m_settings.beginGroup(QString("Task_%1").arg(index++));
        m_settings.setValue("id", task.id);
        m_settings.setValue("url", task.url.toString());
        m_settings.setValue("fileName", task.fileName);
        m_settings.setValue("totalBytes", task.totalBytes);
        m_settings.setValue("downloadedBytes", task.downloadedBytes);
        m_settings.setValue("progress", task.progress);
        m_settings.setValue("speed", task.speed);
        m_settings.setValue("elapsedMillis", task.elapsedMillis);
        m_settings.setValue("state", static_cast<int>(task.state));
        m_settings.setValue("errorMessage", task.errorMessage);
        m_settings.setValue("savePath", task.savePath);
        m_settings.endGroup();
    }

    m_settings.endGroup();
}

void QtNetworkRequest::NetworkDownloadManager::loadTasks()
{
    m_settings.beginGroup("Tasks");

    QStringList taskGroups = m_settings.childGroups();
    for (const QString &group : taskGroups)
    {
        m_settings.beginGroup(group);

        QtNetworkRequest::NetworkDownloadTask task;
        task.id = m_settings.value("id").toString();
        task.url = QUrl(m_settings.value("url").toString());
        task.fileName = m_settings.value("fileName").toString();
        task.totalBytes = m_settings.value("totalBytes").toLongLong();
        task.downloadedBytes = m_settings.value("downloadedBytes").toLongLong();
        task.progress = m_settings.value("progress").toInt();
        task.speed = m_settings.value("speed").toLongLong();
        task.elapsedMillis = m_settings.value("elapsedMillis").toLongLong();
        task.state = static_cast<QtNetworkRequest::NetworkDownloadTask::State>(m_settings.value("state").toInt());
        task.errorMessage = m_settings.value("errorMessage").toString();
        task.savePath = m_settings.value("savePath").toString();

        // Only load incomplete tasks
        if (task.state != QtNetworkRequest::NetworkDownloadTask::State::Completed && task.state != QtNetworkRequest::NetworkDownloadTask::State::Error)
        {
            task.state = QtNetworkRequest::NetworkDownloadTask::State::Waiting; // Reset to waiting state
            addDownloadTaskForUIOnly(task);     // Add to UI but don't auto-start
        }

        m_settings.endGroup();
    }

    m_settings.endGroup();
}

void QtNetworkRequest::NetworkDownloadManager::onDownloadProgress(const QString &taskId, qint64 bytesDownloaded, qint64 bytesTotal)
{
    // Find the download task by taskId
    if (m_downloads.contains(taskId))
    {
        DownloadInfo &info = m_downloads[taskId];
        if (info.isActive)
        {
            info.task.downloadedBytes = bytesDownloaded;
            info.task.totalBytes = bytesTotal;

            if (bytesTotal > 0)
            {
                info.task.progress = static_cast<int>((bytesDownloaded * 100) / bytesTotal);
            }

            emit taskProgress(info.task.id, bytesDownloaded, bytesTotal, info.task.speed);
        }
    }
}

void QtNetworkRequest::NetworkDownloadManager::onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
{
    // Find the download task by requestId
    for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it)
    {
        if (it->requestId == rsp->task.id && it->isActive)
        {
            DownloadInfo &info = *it;
            QString taskId = info.task.id; // Store ID before removing

            info.isActive = false;
            if (info.speedTimer)
            {
				if (info.speedTimer->isActive())
					info.speedTimer->stop();
                info.speedTimer->deleteLater();
                info.speedTimer = nullptr;
            }
            m_activeDownloadCount--;

            if (rsp->isSuccess())
            {
                info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Completed;
                info.task.progress = 100;
                info.task.elapsedMillis = info.downloadTimer.elapsed();
                info.task.speed = info.task.totalBytes * 1000 / info.task.elapsedMillis;

                // ---- Filename fixup ----
                // Try to recover the real filename from server hints:
                //   1) Content-Disposition: attachment; filename="..."
                //   2) X-Final-Url: final URL after redirects (set by the library)
                // Then rename the file on disk and notify the UI.
                QString realName;

                // (1) Content-Disposition (case-insensitive key match)
                for (auto hdrIt = rsp->headers.cbegin(); hdrIt != rsp->headers.cend(); ++hdrIt)
                {
                    if (hdrIt.key().compare("content-disposition", Qt::CaseInsensitive) == 0)
                    {
                        QString cdValue = QString::fromUtf8(hdrIt.value().trimmed());
                        QRegularExpression re("filename\\s*=\\s*(?:\"([^\"]*)\"|([^\\s;]+))",
                                              QRegularExpression::CaseInsensitiveOption);
                        QRegularExpressionMatch m = re.match(cdValue);
                        if (m.hasMatch())
                        {
                            realName = m.captured(1);
                            if (realName.isEmpty())
                                realName = m.captured(2);
                            realName = QUrl::fromPercentEncoding(realName.trimmed().toUtf8());
                        }
                        break;
                    }
                }

                // (2) Fallback: basename of the final URL (after redirects)
                if (realName.isEmpty())
                {
                    for (auto hdrIt = rsp->headers.cbegin(); hdrIt != rsp->headers.cend(); ++hdrIt)
                    {
                        if (hdrIt.key().compare("X-Final-Url", Qt::CaseInsensitive) == 0)
                        {
                            QUrl finalUrl(QString::fromUtf8(hdrIt.value()));
                            if (finalUrl.isValid())
                            {
                                QString baseName = QFileInfo(finalUrl.path()).fileName();
                                if (!baseName.isEmpty() && baseName != "/")
                                    realName = baseName;
                            }
                            break;
                        }
                    }
                }

                if (!realName.isEmpty() && realName != info.task.fileName)
                {
                    QString oldPath = QDir(m_downloadDir).filePath(info.task.fileName);
                    QString newPath = QDir(m_downloadDir).filePath(realName);

                    if (QFile::exists(newPath))
                        QFile::remove(newPath);

                    if (QFile::rename(oldPath, newPath))
                    {
                        info.task.fileName = realName;
                        emit taskFileNameChanged(info.task.id, realName);
                    }
                }

                emit taskStateChanged(info.task.id, QtNetworkRequest::NetworkDownloadTask::State::Completed);
                emit taskElapsedTimeChanged(info.task.id, info.task.elapsedMillis);
                emit taskCompleted(info.task.id, true);
            }
            else
            {
                info.task.state = QtNetworkRequest::NetworkDownloadTask::State::Error;
                info.task.errorMessage = rsp->error.message;
                info.task.speed = 0;
                emit taskStateChanged(info.task.id, QtNetworkRequest::NetworkDownloadTask::State::Error, rsp->error.message);
                emit taskCompleted(info.task.id, false);
            }

            emit activeDownloadsChanged(m_activeDownloadCount);
            
            // Remove task from registry regardless of success or failure
            m_downloads.remove(taskId);
            saveTasks(); // Save updated task list

            // Start next download if available
            startNextDownload();
            break;
        }
    }
}

void QtNetworkRequest::NetworkDownloadManager::startNextDownload()
{
    if (m_activeDownloadCount >= m_maxConcurrentDownloads)
        return;

    // Find waiting tasks to start
    for (auto it = m_downloads.begin(); it != m_downloads.end(); ++it)
    {
        if (!it->isActive && it->task.state == QtNetworkRequest::NetworkDownloadTask::State::Waiting)
        {
            startDownload(it->task.id);
            if (m_activeDownloadCount >= m_maxConcurrentDownloads)
                break;
        }
    }
}

void QtNetworkRequest::NetworkDownloadManager::updateDownloadSpeed(const QString &taskId)
{
    if (!m_downloads.contains(taskId))
        return;

    DownloadInfo &info = m_downloads[taskId];
    if (!info.isActive || info.task.state != QtNetworkRequest::NetworkDownloadTask::State::Running)
        return;

    // Fixed sampling window based on the download timer, decoupled from the
    // sparse progress events emitted by multi-threaded downloads, so the speed
    // no longer collapses to 0 between events.
    const qint64 now = info.downloadTimer.elapsed();       // ms since start
    const qint64 interval = now - info.lastSampleElapsed;   // ms since last sample
    if (interval < 1)
        return;

    const qint64 bytesDiff = info.task.downloadedBytes - info.lastDownloadedBytes;
    const double instant = static_cast<double>(bytesDiff) * 1000.0 / static_cast<double>(interval);

    // Exponential moving average keeps the displayed speed smooth and avoids
    // sudden drops to 0 during brief idle gaps.
    // Bootstrap: on the first sample (or after a reset), jump directly to the
    // instantaneous speed so the displayed value does not slowly climb from 0.
    const double alpha = 0.35;
    if (info.smoothSpeed <= 0.0)
        info.smoothSpeed = instant;
    else
        info.smoothSpeed = alpha * instant + (1.0 - alpha) * info.smoothSpeed;
    info.currentSpeed = static_cast<qint64>(info.smoothSpeed);
    info.lastSampleElapsed = now;
    info.lastDownloadedBytes = info.task.downloadedBytes;

    info.task.speed = info.currentSpeed;

    // Calculate total speed
    qint64 totalSpeed = 0;
    for (const auto &downloadInfo : m_downloads)
    {
        if (downloadInfo.isActive)
        {
            totalSpeed += downloadInfo.currentSpeed;
        }
    }

    emit taskProgress(taskId, info.task.downloadedBytes, info.task.totalBytes, info.task.speed);
    emit downloadSpeedChanged(totalSpeed);
}

void QtNetworkRequest::NetworkDownloadManager::cleanupDownload(const QString &taskId)
{
    if (m_downloads.contains(taskId))
    {
        DownloadInfo &info = m_downloads[taskId];
        if (info.reply)
        {
            info.reply = nullptr;
        }
        if (info.speedTimer)
        {
			if (info.speedTimer->isActive())
				info.speedTimer->stop();
            info.speedTimer->deleteLater();
            info.speedTimer = nullptr;
        }
    }
}

std::unique_ptr<QtNetworkRequest::RequestContext> QtNetworkRequest::NetworkDownloadManager::createRequestTask(const QtNetworkRequest::NetworkDownloadTask &task)
{
    std::unique_ptr<QtNetworkRequest::RequestContext> req = std::make_unique<QtNetworkRequest::RequestContext>();
    req->type = QtNetworkRequest::RequestType::MTDownload;
    req->url = task.url.toString();
    req->downloadConfig = std::make_unique<QtNetworkRequest::DownloadConfig>();
    req->downloadConfig->saveFileName = generateUniqueFilePath(task.fileName);
    req->downloadConfig->saveDir = m_downloadDir;
    req->downloadConfig->overwriteFile = true;
    req->downloadConfig->threadCount = m_maxThreads;
    req->behavior.showProgress = true;
    req->behavior.retryOnFailed = true;
    return req;
}

QString QtNetworkRequest::NetworkDownloadManager::generateUniqueFilePath(const QString &fileName) const
{
    QString filePath = QDir(m_downloadDir).filePath(fileName);
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists())
    {
        return fileName;
    }

    // Generate unique filename by adding suffix
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();
    int counter = 1;

    while (true)
    {
        QString newFileName;
        if (extension.isEmpty())
        {
            newFileName = QString("%1_%2").arg(baseName).arg(counter);
        }
        else
        {
            newFileName = QString("%1_%2.%3").arg(baseName).arg(counter).arg(extension);
        }

        QString newFilePath = QDir(m_downloadDir).filePath(newFileName);
        if (!QFileInfo::exists(newFilePath))
        {
            return newFileName;
        }

        counter++;
    }
}