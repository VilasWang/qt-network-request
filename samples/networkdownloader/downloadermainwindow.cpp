#include "downloadermainwindow.h"
#include "ui_NetworkDownloaderMainWindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDialogButtonBox>

QtNetworkRequest::NetworkDownloaderMainWindow::NetworkDownloaderMainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::DownloaderMainWindow), m_settings("QtDownloader", "MainWindow")
{
    ui->setupUi(this);

    // Set window properties
    setWindowTitle("Qt Downloader - Modern Download Manager");
    setWindowIcon(QIcon(":/icons/app.ico")); // Set app icon if available

    // Set modern window properties
    setMinimumSize(900, 600);
    resize(1200, 800);

    // Initialize models and managers
    m_taskModel = new QtNetworkRequest::NetworkDownloadTaskModel(this);
    m_downloadManager = new QtNetworkRequest::NetworkDownloadManager(this);

    // Setup table view with modern styling
    ui->tableViewTasks->setModel(m_taskModel);

    // Configure modern table view
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableViewTasks->horizontalHeader()->setStretchLastSection(false);

    // Set column resize modes - File Name auto expands, others fixed
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnFileName), QHeaderView::Stretch);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnFileSize), QHeaderView::Fixed);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnDownloaded), QHeaderView::Fixed);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnProgress), QHeaderView::Fixed);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnSpeed), QHeaderView::Fixed);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnTime), QHeaderView::Fixed);
    ui->tableViewTasks->horizontalHeader()->setSectionResizeMode(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnState), QHeaderView::Fixed);

    // Set fixed widths for non-expanding columns
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnFileSize), 100);   // File size - fixed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnDownloaded), 100); // Downloaded - fixed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnProgress), 120);   // Progress - fixed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnSpeed), 100);      // Speed - fixed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnTime), 80);        // Time - fixed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnState), 120);      // State - fixed

    ui->tableViewTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTasks->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewTasks->setAlternatingRowColors(true);
    ui->tableViewTasks->setShowGrid(true);
    ui->tableViewTasks->setGridStyle(Qt::DotLine);

    // Enable word wrap for better text display
    ui->tableViewTasks->setWordWrap(false);
    ui->tableViewTasks->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Setup connections
    setupConnections();

    // Load settings
    loadGeometrySettings();

    // Update UI
    updateUI();

    // Initialize notification system
    m_notificationYOffset = 175; // Start below the button area (150px + 25px margin)
    m_notificationTimer.setSingleShot(true);
    connect(&m_notificationTimer, &QTimer::timeout, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onNotificationTimeout);
}

QtNetworkRequest::NetworkDownloaderMainWindow::~NetworkDownloaderMainWindow()
{
    // Stop any running timers to prevent crashes
    m_notificationTimer.stop();

    // Clear all notifications to prevent crashes
    clearNotifications();

    // Save settings
    saveGeometrySettings();

    // Delete UI
    delete ui;
}

void QtNetworkRequest::NetworkDownloaderMainWindow::closeEvent(QCloseEvent *event)
{
    // Check if there are active downloads
    if (m_taskModel->getRunningTaskCount() > 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Confirm Exit",
            "There are active downloads. Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            event->ignore();
            return;
        }
    }

    event->accept();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Reposition existing notifications when window is resized
    if (!m_notifications.isEmpty())
    {
        int buttonAreaHeight = 150; // Approximate height of the URL input and button area
        int yOffset = buttonAreaHeight + 25;
        for (QLabel *notification : m_notifications)
        {
            if (notification && notification->isVisible())
            {
                int x = this->width() - notification->width() - 25;
                int y = yOffset;

                // Ensure notification stays within window bounds and doesn't overlap with header area
                x = qMax(25, x);
                y = qMax(buttonAreaHeight + 25, y);

                notification->move(x, y);
                yOffset += notification->height() + 15; // Increased spacing between notifications
            }
        }
        m_notificationYOffset = yOffset;
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::setupConnections()
{
    // Button connections
    connect(ui->btnAddTasks, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onAddTasksClicked);
    connect(ui->btnStart, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onStartClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onCancelClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onDeleteClicked);

    // Table selection
    connect(ui->tableViewTasks->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskSelectionChanged);

    // Download manager connections
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskAdded, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskAdded);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskProgress, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskProgress);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskElapsedTimeChanged, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskElapsedTimeChanged);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskStateChanged, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskStateChanged);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskCompleted, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskCompleted);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::downloadSpeedChanged, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onDownloadSpeedChanged);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::activeDownloadsChanged, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onActiveDownloadsChanged);

    // Menu actions
    connect(ui->actionSettings, &QAction::triggered, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onActionSettings);
    connect(ui->actionAbout, &QAction::triggered, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onActionAbout);
    connect(ui->actionExit, &QAction::triggered, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onActionExit);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onAddTasksClicked()
{
    QString urlsText = ui->plainTextEditUrls->toPlainText().trimmed();
    if (urlsText.isEmpty())
    {
        showNotification("Warning: Please enter at least one URL", "warning", 2000);
        return;
    }

    QStringList urls = urlsText.split('\n', QString::SkipEmptyParts);
    int addedCount = 0;

    for (const QString &urlStr : urls)
    {
        QUrl url(urlStr.trimmed());
        if (url.isValid())
        {
            QtNetworkRequest::NetworkDownloadTask task(url, m_downloadManager->getDownloadDirectory());
            m_downloadManager->addDownloadTask(task);
            addedCount++;
        }
    }

    if (addedCount > 0)
    {
        ui->plainTextEditUrls->clear();
        showNotification(QString("Added %1 download task(s)").arg(addedCount), "success", 3000);
    }
    else
    {
        showNotification("Warning: No valid URLs found", "warning", 3000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onStartClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        showNotification("Warning: Please select a task to start", "warning", 2000);
        return;
    }

    QModelIndex index = selected.first();
    QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());

    if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running)
    {
        showNotification("Info: Task is already running", "info", 2000);
        return;
    }

    if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Completed)
    {
        showNotification("Success: Task is already completed", "success", 2000);
        return;
    }

    m_downloadManager->startDownload(task.id);
    showNotification("Download started", "success", 2000);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onCancelClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        showNotification("Warning: Please select a task to cancel", "warning", 2000);
        return;
    }

    QModelIndex index = selected.first();
    QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());

    if (task.state != QtNetworkRequest::NetworkDownloadTask::State::Running)
    {
        showNotification("Info: Task is not running", "info", 2000);
        return;
    }

    m_downloadManager->pauseDownload(task.id);
    showNotification("Download paused", "info", 2000);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onDeleteClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        QMessageBox::warning(this, "Warning", "Please select a task to delete.");
        return;
    }

    QModelIndex index = selected.first();
    QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete",
        QString("Are you sure you want to delete the task '%1'?").arg(task.fileName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Block signals during deletion to prevent crashes
        ui->tableViewTasks->setUpdatesEnabled(false);
        ui->tableViewTasks->selectionModel()->blockSignals(true);

        // Clear selection first to prevent crashes
        ui->tableViewTasks->selectionModel()->clearSelection();

        // Remove from model first (this will update the UI immediately)
        m_taskModel->removeTask(task.id);

        // Then remove from download manager
        m_downloadManager->removeDownload(task.id);

        // Re-enable signals and updates
        ui->tableViewTasks->selectionModel()->blockSignals(false);
        ui->tableViewTasks->setUpdatesEnabled(true);

        // Update UI after deletion
        updateUI();
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskSelectionChanged()
{
    bool hasSelection = !ui->tableViewTasks->selectionModel()->selectedRows().isEmpty();
    ui->btnStart->setEnabled(hasSelection);
    ui->btnCancel->setEnabled(hasSelection);
    ui->btnDelete->setEnabled(hasSelection);

    if (hasSelection)
    {
        QModelIndex index = ui->tableViewTasks->selectionModel()->selectedRows().first();
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());

        ui->btnStart->setEnabled(task.state == QtNetworkRequest::NetworkDownloadTask::State::Waiting || task.state == QtNetworkRequest::NetworkDownloadTask::State::Paused);
        ui->btnCancel->setEnabled(task.state == QtNetworkRequest::NetworkDownloadTask::State::Running);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskAdded(const QtNetworkRequest::NetworkDownloadTask &task)
{
    m_taskModel->addTask(task);
    updateUI();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskProgress(const QString &taskId, qint64 downloaded, qint64 total, qint64 speed)
{
    m_taskModel->updateTaskProgress(taskId, downloaded, total, speed);
    updateGlobalSpeed();
    updateTimeRemaining();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskElapsedTimeChanged(const QString &taskId, qint64 elapsedMillis)
{
    m_taskModel->updateTaskElapsedTime(taskId, elapsedMillis);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskStateChanged(const QString &taskId, QtNetworkRequest::NetworkDownloadTask::State state, const QString &error)
{
    m_taskModel->updateTaskState(taskId, state, error);
    updateUI();

    if (state == QtNetworkRequest::NetworkDownloadTask::State::Error)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(taskId);
        showNotification(QString("Download failed: %1").arg(task.fileName), "error", 5000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskCompleted(const QString &taskId, bool success)
{
    if (success)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(taskId);
        m_taskModel->updateTaskTotalSpeed(taskId);
        showNotification(QString("'%1' downloaded successfully").arg(task.fileName), "success", 4000);
    }
    updateUI();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onDownloadSpeedChanged(qint64 totalSpeed)
{
    QString speedText;
    if (totalSpeed < 1024)
    {
        speedText = QString("Speed: %1 B/s").arg(totalSpeed);
    }
    else if (totalSpeed < 1024 * 1024)
    {
        speedText = QString("Speed: %1 KB/s").arg(totalSpeed / 1024);
    }
    else
    {
        double speedMB = static_cast<double>(totalSpeed) / (1024.0 * 1024.0);
        speedText = QString("Speed: %1 MB/s").arg(speedMB, 0, 'f', 1);
    }

    ui->labelSpeed->setText(speedText);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActiveDownloadsChanged(int count)
{
    Q_UNUSED(count);
    updateUI();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionSettings()
{
    showSettingsDialog();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionAbout()
{
    showAboutDialog();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionExit()
{
    close();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::updateUI()
{
    int runningCount = m_taskModel->getRunningTaskCount();
    int totalCount = m_taskModel->rowCount();

    QString statusText = QString("Tasks: %1 total, %2 running").arg(totalCount).arg(runningCount);
    ui->statusBar->showMessage(statusText);

    onTaskSelectionChanged(); // Update button states
}

void QtNetworkRequest::NetworkDownloaderMainWindow::updateGlobalSpeed()
{
    qint64 totalSpeed = m_taskModel->getTotalSpeed();
    onDownloadSpeedChanged(totalSpeed);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::updateTimeRemaining()
{
    qint64 totalSpeed = m_taskModel->getTotalSpeed();
    qint64 totalSize = m_taskModel->getTotalSize();
    qint64 totalDownloaded = m_taskModel->getTotalDownloaded();

    if (totalSpeed > 0 && totalSize > 0)
    {
        qint64 remainingBytes = totalSize - totalDownloaded;
        qint64 remainingSeconds = remainingBytes / totalSpeed;

        QString timeText;
        if (remainingSeconds < 60)
        {
            timeText = QString("Time: %1s").arg(remainingSeconds);
        }
        else if (remainingSeconds < 3600)
        {
            int minutes = remainingSeconds / 60;
            int seconds = remainingSeconds % 60;
            timeText = QString("Time: %1m %2s").arg(minutes).arg(seconds);
        }
        else
        {
            int hours = remainingSeconds / 3600;
            int minutes = (remainingSeconds % 3600) / 60;
            timeText = QString("Time: %1h %2m").arg(hours).arg(minutes);
        }

        ui->labelTime->setText(timeText);
    }
    else
    {
        ui->labelTime->setText("Time: --");
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::showSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    dialog.setModal(true);
    dialog.setMinimumSize(500, 300);

    // Apply modern dark theme styling to dialog
    dialog.setStyleSheet(R"(
        QDialog {
            background-color: #1a1a1a;
            color: #ffffff;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QLabel {
            color: #ffffff;
            font-size: 13px;
            font-weight: 600;
        }
        QLineEdit {
            border: 2px solid #404040;
            border-radius: 8px;
            padding: 12px;
            background-color: #2d2d2d;
            color: #ffffff;
            font-size: 13px;
            font-family: 'Segoe UI', Arial, sans-serif;
        }
        QLineEdit:focus {
            border: 2px solid #4a9eff;
            outline: none;
        }
        QSpinBox {
            border: 2px solid #404040;
            border-radius: 8px;
            padding: 12px;
            background-color: #2d2d2d;
            color: #ffffff;
            font-size: 13px;
            font-family: 'Segoe UI', Arial, sans-serif;
            min-width: 100px;
            min-height: 36px;
        }
        QFrame {
            background-color: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 12px;
        }
        QPushButton {
            background-color: #4a9eff;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-size: 13px;
            font-weight: 600;
            font-family: 'Segoe UI', Arial, sans-serif;
            min-width: 100px;
            min-height: 36px;
        }
        QPushButton:hover {
            background-color: #357abd;
            border: 1px solid #5aafff;
        }
        QPushButton:pressed {
            background-color: #2968a3;
            border: 1px solid #4a9eff;
        }
        QPushButton#cancelButton {
            background-color: #ff4757;
        }
        QPushButton#cancelButton:hover {
            background-color: #ff3838;
            border: 1px solid #ff6b7a;
        }
        QPushButton#cancelButton:pressed {
            background-color: #ff2727;
            border: 1px solid #ff4757;
        }
    )");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setMargin(20);

    // Download directory
    QFrame *dirFrame = new QFrame();
    dirFrame->setFrameStyle(QFrame::StyledPanel);
    dirFrame->setStyleSheet("QFrame { border-radius: 12px; }");
    QVBoxLayout *dirFrameLayout = new QVBoxLayout(dirFrame);
    dirFrameLayout->setMargin(16);

    QLabel *dirLabel = new QLabel("Download Directory:");
    QHBoxLayout *dirLayout = new QHBoxLayout();
    QLineEdit *dirEdit = new QLineEdit(m_downloadManager->getDownloadDirectory());
    QPushButton *dirButton = new QPushButton("Browse...");

    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(dirButton);
    dirFrameLayout->addWidget(dirLabel);
    dirFrameLayout->addLayout(dirLayout);

    // Max threads
    QFrame *threadFrame = new QFrame();
    threadFrame->setFrameStyle(QFrame::StyledPanel);
    threadFrame->setStyleSheet("QFrame { border-radius: 12px; }");
    QVBoxLayout *threadFrameLayout = new QVBoxLayout(threadFrame);
    threadFrameLayout->setMargin(16);

    QLabel *threadLabel = new QLabel("Max Threads per Download:");
    QHBoxLayout *threadLayout = new QHBoxLayout();
    QSpinBox *threadSpinBox = new QSpinBox();
    threadSpinBox->setRange(1, 64);
    threadSpinBox->setValue(m_downloadManager->getMaxThreads());
    threadSpinBox->setToolTip("Number of threads to use for each download (1-64)");

    threadLayout->addWidget(threadSpinBox);
    threadLayout->addStretch();
    threadFrameLayout->addWidget(threadLabel);
    threadFrameLayout->addLayout(threadLayout);

    // Max concurrent downloads
    QFrame *concurrentFrame = new QFrame();
    concurrentFrame->setFrameStyle(QFrame::StyledPanel);
    concurrentFrame->setStyleSheet("QFrame { border-radius: 12px; }");
    QVBoxLayout *concurrentFrameLayout = new QVBoxLayout(concurrentFrame);
    concurrentFrameLayout->setMargin(16);

    QLabel *concurrentLabel = new QLabel("Max Concurrent Downloads:");
    QHBoxLayout *concurrentLayout = new QHBoxLayout();
    QSpinBox *concurrentSpinBox = new QSpinBox();
    concurrentSpinBox->setRange(1, 20);
    concurrentSpinBox->setValue(m_downloadManager->getMaxConcurrentDownloads());
    concurrentSpinBox->setToolTip("Maximum number of downloads running at the same time (1-20)");

    concurrentLayout->addWidget(concurrentSpinBox);
    concurrentLayout->addStretch();
    concurrentFrameLayout->addWidget(concurrentLabel);
    concurrentFrameLayout->addLayout(concurrentLayout);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("OK");
    QPushButton *cancelButton = new QPushButton("Cancel");
    cancelButton->setObjectName("cancelButton");

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    layout->addWidget(dirFrame);
    layout->addWidget(threadFrame);
    layout->addWidget(concurrentFrame);
    layout->addStretch();
    layout->addLayout(buttonLayout);

    // Connections
    connect(dirButton, &QPushButton::clicked, [=]()
            {
        QString dir = QFileDialog::getExistingDirectory(nullptr, "Select Download Directory", dirEdit->text());
        if (!dir.isEmpty()) {
            dirEdit->setText(dir);
        } });

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_downloadManager->setDownloadDirectory(dirEdit->text());
        m_downloadManager->setMaxThreads(threadSpinBox->value());
        m_downloadManager->setMaxConcurrentDownloads(concurrentSpinBox->value());
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::showAboutDialog()
{
    QMessageBox::about(this, "About Qt Downloader",
                       "<div style='color: #ffffff; font-family: \"Segoe UI\", Arial, sans-serif;'>"
                       "<h3 style='color: #0078d4; margin: 10px 0;'>Qt Downloader v1.0</h3>"
                       "<p style='color: #cccccc; margin: 15px 0;'>A modern multi-threaded download manager built with Qt</p>"
                       "<div style='background-color: #252526; border: 1px solid #3e3e42; border-radius: 4px; padding: 15px; margin: 15px 0;'>"
                       "<h4 style='color: #ffffff; margin: 0 0 10px 0;'>🚀 Features</h4>"
                       "<ul style='color: #cccccc; margin: 0;'>"
                       "<li>Multi-threaded downloads (up to 64 threads)</li>"
                       "<li>Batch URL processing</li>"
                       "<li>Real-time progress tracking</li>"
                       "<li>Pause/Resume functionality</li>"
                       "<li>Speed monitoring & statistics</li>"
                       "<li>Modern dark theme UI design</li>"
                       "</ul>"
                       "</div>"
                       "<div style='background-color: #252526; border: 1px solid #3e3e42; border-radius: 4px; padding: 15px; margin: 15px 0;'>"
                       "<h4 style='color: #ffffff; margin: 0 0 10px 0;'>⚙️ Technical Details</h4>"
                       "<ul style='color: #cccccc; margin: 0;'>"
                       "<li>Built with Qt5 framework</li>"
                       "<li>Uses QNetworkRequest library</li>"
                       "<li>Supports HTTP/HTTPS/FTP protocols</li>"
                       "<li>Cross-platform compatibility</li>"
                       "</ul>"
                       "</div>"
                       "<div style='background-color: #252526; border: 1px solid #3e3e42; border-radius: 4px; padding: 15px; margin: 15px 0;'>"
                       "<h4 style='color: #ffffff; margin: 0 0 10px 0;'>📄 License</h4>"
                       "<p style='color: #cccccc; margin: 0;'>LGPL v3.0</p>"
                       "<p style='color: #808080; margin: 10px 0 0 0; font-size: 12px;'>© 2024 QtNetworkRequest Team</p>"
                       "</div>"
                       "</div>");
}

void QtNetworkRequest::NetworkDownloaderMainWindow::saveGeometrySettings()
{
    try
    {
        m_settings.setValue("geometry", saveGeometry());
        m_settings.setValue("windowState", saveState());
        m_settings.sync(); // Ensure data is written immediately
    }
    catch (...)
    {
        // Silently ignore settings errors to prevent crashes
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::loadGeometrySettings()
{
    try
    {
        restoreGeometry(m_settings.value("geometry").toByteArray());
        restoreState(m_settings.value("windowState").toByteArray());
    }
    catch (...)
    {
        // Silently ignore settings errors to prevent crashes
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::showNotification(const QString &message, const QString &type, int duration)
{
    // Clear any existing notifications to prevent stacking
    clearNotifications();

    // Create notification label with proper text wrapping
    QLabel *notification = new QLabel(message, this);
    notification->setObjectName("notification");
    notification->setAlignment(Qt::AlignCenter);
    notification->setWordWrap(true); // Enable word wrap for long messages
    notification->setStyleSheet(QString(
                                    "QLabel#notification {"
                                    "   background-color: %1;"
                                    "   color: white;"
                                    "   border: 2px solid rgba(255, 255, 255, 0.3);"
                                    "   border-radius: 12px;"
                                    "   padding: 14px 24px;"
                                    "   font-family: 'Segoe UI', Arial, sans-serif;"
                                    "   font-size: 14px;"
                                    "   font-weight: 600;"
                                    "   margin: 8px;"
                                    "   line-height: 1.5;"
                                    "   box-shadow: 0 4px 15px rgba(0, 0, 0, 0.3);"
                                    "}")
                                    .arg(getNotificationColor(type)));

    // Calculate appropriate size based on content
    QFontMetrics fm(notification->font());
    int maxWidth = qMin(500, this->width() - 60); // Max 500px or window width minus margins
    int textWidth = fm.width(message);

    // If text is longer than max width, enable word wrap and set fixed width
    if (textWidth > maxWidth - 40)
    { // 40px for padding
        notification->setFixedWidth(maxWidth);
        notification->setWordWrap(true);
    }
    else
    {
        notification->setMinimumWidth(textWidth + 40); // Add padding
        notification->setMaximumWidth(maxWidth);
    }

    // Set minimum height to accommodate text
    notification->setMinimumHeight(40);

    // Force size calculation
    notification->adjustSize();

    // Position notification in top-right corner with proper margins
    // Add extra offset to avoid overlapping with the Add Task button area
    int buttonAreaHeight = 150; // Approximate height of the URL input and button area
    int x = this->width() - notification->width() - 25;
    int y = m_notificationYOffset + buttonAreaHeight - 60;

    // Ensure notification stays within window bounds and doesn't overlap with header area
    x = qMax(25, x);
    y = qMax(buttonAreaHeight + 25, y);

    notification->move(x, y);

    // Show notification with fade-in effect
    notification->show();

    // Create fade-in animation with error handling
    QPropertyAnimation *fadeIn = new QPropertyAnimation(notification, "windowOpacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    // Connect error handling
    connect(fadeIn, &QPropertyAnimation::finished, [fadeIn]()
            { fadeIn->deleteLater(); });

    fadeIn->start();

    // Store notification
    m_notifications.append(notification);

    // Set timer to hide notification with error handling
    if (duration > 0)
    {
        m_notificationTimer.start(duration);
    }

    // Update Y offset for next notification
    m_notificationYOffset += notification->height() + 10;
}

void QtNetworkRequest::NetworkDownloaderMainWindow::hideNotification()
{
    if (m_notifications.isEmpty())
    {
        return;
    }

    QLabel *notification = m_notifications.takeFirst();
    if (!notification)
    {
        return;
    }

    try
    {
        // Create fade-out animation with error handling
        QPropertyAnimation *fadeOut = new QPropertyAnimation(notification, "windowOpacity");
        fadeOut->setDuration(200);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);

        // Delete notification after animation
        connect(fadeOut, &QPropertyAnimation::finished, [notification, fadeOut]()
                {
            if (notification) notification->deleteLater();
            if (fadeOut) fadeOut->deleteLater(); });

        fadeOut->start();
    }
    catch (...)
    {
        // If animation fails, hide and delete immediately
        notification->hide();
        notification->deleteLater();
    }

    // Reset Y offset
    m_notificationYOffset = 175; // Reset to below button area
}

void QtNetworkRequest::NetworkDownloaderMainWindow::clearNotifications()
{
    // Safely clear all existing notifications
    while (!m_notifications.isEmpty())
    {
        QLabel *notification = m_notifications.takeFirst();
        if (notification)
        {
            notification->hide();
            notification->deleteLater();
        }
    }
    m_notificationTimer.stop();
    m_notificationYOffset = 175; // Reset to below button area
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onNotificationTimeout()
{
    try
    {
        hideNotification();
    }
    catch (...)
    {
        // Silently ignore timeout errors to prevent crashes
        m_notificationTimer.stop();
    }
}

QString QtNetworkRequest::NetworkDownloaderMainWindow::getNotificationColor(const QString &type)
{
    if (type == "success")
    {
        return "#27ae60"; // Deeper green to distinguish from add button
    }
    else if (type == "warning")
    {
        return "#e67e22"; // Deeper orange to distinguish from add button
    }
    else if (type == "error")
    {
        return "#e74c3c"; // Deeper red
    }
    else
    {
        return "#3498db"; // Deeper blue to distinguish from add button
    }
}