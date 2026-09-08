#include "downloadermainwindow.h"
#include "ui_NetworkDownloaderMainWindow.h"
#include "thememanager.h"
#include "tasktabledelegate.h"
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
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTimer>

QtNetworkRequest::NetworkDownloaderMainWindow::NetworkDownloaderMainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::DownloaderMainWindow), m_settings("QtDownloader", "MainWindow")
{
    ui->setupUi(this);

    // Set window properties
    setWindowTitle("Qt Downloader");
    setWindowIcon(QIcon(":/icons/app.ico")); // Set app icon if available

    // Set modern window properties
    setMinimumSize(900, 600);
    resize(1200, 800);

    // Initialize models and managers
    m_taskModel = new QtNetworkRequest::NetworkDownloadTaskModel(this);
    m_downloadManager = new QtNetworkRequest::NetworkDownloadManager(this);

    // Setup table view with modern styling
    ui->tableViewTasks->setModel(m_taskModel);

    // Prototype-faithful cell rendering: file icon square + name/URL,
    // thin rounded progress bar and pill-shaped status badges.
    ui->tableViewTasks->setItemDelegate(new QtNetworkRequest::TaskTableDelegate(ui->tableViewTasks));

    // The URL card hugs its content (header + fixed-height text area) so the
    // task table gets all remaining vertical space, like the prototype.
    ui->urlFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

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

    // Set fixed widths for non-expanding columns (matching the prototype colgroup)
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnFileSize), 90);    // File size
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnDownloaded), 110); // Downloaded
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnProgress), 180);   // Progress bar + %
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnSpeed), 100);      // Speed
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnTime), 85);        // Time
    ui->tableViewTasks->setColumnWidth(static_cast<int>(QtNetworkRequest::NetworkDownloadTaskModel::Column::ColumnState), 120);      // Status badge

    ui->tableViewTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTasks->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Selecting a (full) row otherwise highlights the column header sections,
    // making the header bar look selected/blue. Disable section highlighting so
    // only the row is selected.
    ui->tableViewTasks->horizontalHeader()->setHighlightSections(false);
    ui->tableViewTasks->verticalHeader()->setHighlightSections(false);
    ui->tableViewTasks->setAlternatingRowColors(false);
    ui->tableViewTasks->setShowGrid(true);
    ui->tableViewTasks->setGridStyle(Qt::SolidLine);
    // Tall rows so the two-line file cell / badge have breathing room.
    ui->tableViewTasks->verticalHeader()->setDefaultSectionSize(52);

    // Enable word wrap for better text display
    ui->tableViewTasks->setWordWrap(false);
    ui->tableViewTasks->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Status bar: running stats on the left, download directory on the right
    // (mirrors the prototype status bar).
    m_labelDownloadDir = new QLabel(this);
    m_labelDownloadDir->setObjectName(QStringLiteral("lblDownloadDir"));
    m_labelDownloadDir->setText(QStringLiteral("下载目录: %1").arg(m_downloadManager->getDownloadDirectory()));
    statusBar()->addPermanentWidget(m_labelDownloadDir);

    // Setup connections
    setupConnections();

    // Theme: create the manager, apply the persisted mode, and wire the
    // View-menu actions (exclusive group) + toolbar toggle button.
    m_theme = new ThemeManager(this);
    m_theme->setMode(m_theme->mode());

    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    m_themeGroup->addAction(ui->actionThemeLight);
    m_themeGroup->addAction(ui->actionThemeDark);
    m_themeGroup->addAction(ui->actionThemeSystem);
    syncThemeActionGroup();
    connect(ui->actionThemeLight, &QAction::triggered, this, &NetworkDownloaderMainWindow::onActionThemeLight);
    connect(ui->actionThemeDark, &QAction::triggered, this, &NetworkDownloaderMainWindow::onActionThemeDark);
    connect(ui->actionThemeSystem, &QAction::triggered, this, &NetworkDownloaderMainWindow::onActionThemeSystem);
    connect(ui->actionToggleTheme, &QAction::triggered, this, &NetworkDownloaderMainWindow::onActionToggleTheme);
    connect(m_theme, &ThemeManager::modeChanged, this, [this](ThemeManager::Mode){ syncThemeActionGroup(); });

    // Inject a compact theme-toggle button into the status frame (right side,
    // next to labelTime) so users can switch light/dark without opening the
    // View menu — parity with the QtRequester toolbar toggle.
    if (auto *statusLayout = qobject_cast<QHBoxLayout *>(ui->statusFrame->layout()))
    {
        auto *btnTheme = new QPushButton(QStringLiteral("\xe2\x98\xbe"));  // ☾
        btnTheme->setObjectName(QStringLiteral("btn_theme"));
        btnTheme->setToolTip(QStringLiteral("切换明暗主题"));
        btnTheme->setFixedSize(28, 28);
        statusLayout->addStretch();
        statusLayout->addWidget(btnTheme);
        connect(btnTheme, &QPushButton::clicked, m_theme, &ThemeManager::toggle);
    }

    // Load settings
    loadGeometrySettings();

    // Update UI
    updateUI();

    // Demo mode (QT_DOWNLOADER_DEMO=1): inject sample tasks that mirror the
    // design prototype so the UI can be compared against the HTML mockup.
    if (qEnvironmentVariableIsSet("QT_DOWNLOADER_DEMO"))
    {
        using Task = QtNetworkRequest::NetworkDownloadTask;
        Task t1(QUrl("https://releases.ubuntu.com/22.04.3/ubuntu-22.04.3-desktop-amd64.iso"));
        t1.totalBytes = 5067982438;
        t1.downloadedBytes = 1965022306;
        t1.progress = 39;
        t1.speed = 8598323;
        t1.elapsedMillis = 228000;
        t1.state = Task::State::Running;
        m_taskModel->addTask(t1);

        Task t2(QUrl("https://github.com/torvalds/linux/archive/refs/tags/v6.5.tar.gz"));
        t2.totalBytes = 234881024;
        t2.downloadedBytes = 163577856;
        t2.progress = 70;
        t2.speed = 5347737;
        t2.elapsedMillis = 31000;
        t2.state = Task::State::Running;
        m_taskModel->addTask(t2);

        Task t3(QUrl("https://docs.example.com/user-guide-v3.pdf"));
        t3.totalBytes = 13421773;
        t3.state = Task::State::Waiting;
        m_taskModel->addTask(t3);

        ui->labelSpeed->setText(QStringLiteral("13.3 MB/s"));
        ui->labelTime->setText(QStringLiteral("4m 52s"));
        updateUI();

        // Self-capture for visual QA: render the window (at native device
        // pixel ratio) to a PNG next to the executable, then exit. Triggered
        // by QT_DOWNLOADER_DEMO so CI/manual comparison against the HTML
        // prototype is reproducible on high-DPI screens.
        QTimer::singleShot(2000, this, [this]() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
            const QString name = qEnvironmentVariable("QT_DOWNLOADER_SHOT");
#else
            // qEnvironmentVariable() was added in Qt 5.10.
            const QString name = QString::fromLocal8Bit(qgetenv("QT_DOWNLOADER_SHOT"));
#endif
            if (!name.isEmpty())
            {
                const QPixmap shot = this->grab();
                shot.save(name, "PNG");
                QCoreApplication::exit(0);
            }
        });
    }

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
#ifdef QT_MTNETWORK_UNIT_TEST
        // In test mode: skip the modal dialog — it blocks the event loop in
        // headless/CI runs and there are no real downloads to confirm anyway.
        event->accept();
        return;
#else
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Confirm Exit",
            "There are active downloads. Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            event->ignore();
            return;
        }
#endif
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
    connect(ui->btnPause, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onPauseClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onCancelClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onDeleteClicked);
    connect(ui->btnSelectAll, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onSelectAllClicked);
    connect(ui->btnClearCompleted, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onClearCompletedClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onActionSettings);

    // Table selection
    connect(ui->tableViewTasks->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskSelectionChanged);

    // Download manager connections
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskAdded, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskAdded);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskProgress, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskProgress);
    connect(m_downloadManager, &QtNetworkRequest::NetworkDownloadManager::taskFileNameChanged, this, &QtNetworkRequest::NetworkDownloaderMainWindow::onTaskFileNameChanged);
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
        showNotification("警告：请至少输入一个下载链接", "warning", 2000);
        return;
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QStringList urls = urlsText.split('\n', Qt::SkipEmptyParts);
#else
    QStringList urls = urlsText.split('\n', QString::SkipEmptyParts);
#endif
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
        showNotification(QStringLiteral("已添加 %1 个任务到队列").arg(addedCount), "success", 3000);
    }
    else
    {
        showNotification("警告：未找到有效的下载链接", "warning", 3000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onStartClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        showNotification("警告：请先选择要开始的任务", "warning", 2000);
        return;
    }

    int startedCount = 0;
    for (const QModelIndex &index : selected)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Waiting ||
            task.state == QtNetworkRequest::NetworkDownloadTask::State::Paused)
        {
            m_downloadManager->startDownload(task.id);
            startedCount++;
        }
    }

    if (startedCount > 0)
    {
        showNotification(QStringLiteral("已开始 %1 个下载").arg(startedCount), "success", 2000);
    }
    else
    {
        showNotification("提示：所选任务无法开始（已在运行或已完成）", "info", 2000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onPauseClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        showNotification("警告：请先选择要暂停的任务", "warning", 2000);
        return;
    }

    int pausedCount = 0;
    for (const QModelIndex &index : selected)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running)
        {
            m_downloadManager->pauseDownload(task.id);
            pausedCount++;
        }
    }

    if (pausedCount > 0)
    {
        showNotification(QStringLiteral("已暂停 %1 个下载").arg(pausedCount), "info", 2000);
    }
    else
    {
        showNotification("提示：所选任务未在运行中", "info", 2000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onCancelClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        showNotification("警告：请先选择要取消的任务", "warning", 2000);
        return;
    }

    int cancelledCount = 0;
    for (const QModelIndex &index : selected)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running ||
            task.state == QtNetworkRequest::NetworkDownloadTask::State::Paused ||
            task.state == QtNetworkRequest::NetworkDownloadTask::State::Waiting)
        {
            m_downloadManager->cancelDownload(task.id);
            cancelledCount++;
        }
    }

    if (cancelledCount > 0)
    {
        showNotification(QStringLiteral("已取消 %1 个下载").arg(cancelledCount), "info", 2000);
    }
    else
    {
        showNotification("提示：所选任务无法取消", "info", 2000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onDeleteClicked()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请先选择要删除的任务。");
        return;
    }

    const int count = selected.size();
    const QString question = (count == 1)
        ? QStringLiteral("确定要删除任务 '%1' 吗？").arg(m_taskModel->getTask(selected.first().row()).fileName)
        : QStringLiteral("确定要删除所选的 %1 个任务吗？").arg(count);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", question, QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Block signals during deletion to prevent crashes
        ui->tableViewTasks->setUpdatesEnabled(false);
        ui->tableViewTasks->selectionModel()->blockSignals(true);

        // Clear selection first to prevent crashes
        ui->tableViewTasks->selectionModel()->clearSelection();

        // Collect ids first — removing rows invalidates the model indexes.
        QStringList ids;
        for (const QModelIndex &index : selected)
        {
            ids.append(m_taskModel->getTask(index.row()).id);
        }
        for (const QString &id : ids)
        {
            m_taskModel->removeTask(id);
            m_downloadManager->removeDownload(id);
        }

        // Re-enable signals and updates
        ui->tableViewTasks->selectionModel()->blockSignals(false);
        ui->tableViewTasks->setUpdatesEnabled(true);

        // Update UI after deletion
        updateUI();
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onSelectAllClicked()
{
    ui->tableViewTasks->selectAll();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onClearCompletedClicked()
{
    const QVector<QtNetworkRequest::NetworkDownloadTask> tasks = m_taskModel->getAllTasks();
    QStringList ids;
    for (const QtNetworkRequest::NetworkDownloadTask &task : tasks)
    {
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Completed)
        {
            ids.append(task.id);
        }
    }

    if (ids.isEmpty())
    {
        showNotification("提示：没有已完成的任务", "info", 2000);
        return;
    }

    ui->tableViewTasks->selectionModel()->clearSelection();
    for (const QString &id : ids)
    {
        m_taskModel->removeTask(id);
        m_downloadManager->removeDownload(id);
    }
    showNotification(QStringLiteral("已清除 %1 个已完成任务").arg(ids.size()), "success", 2000);
    updateUI();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskSelectionChanged()
{
    QModelIndexList selected = ui->tableViewTasks->selectionModel()->selectedRows();
    const bool hasSelection = !selected.isEmpty();

    bool canStart = false;
    bool canPause = false;
    bool canCancel = false;
    for (const QModelIndex &index : selected)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_taskModel->getTask(index.row());
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Waiting ||
            task.state == QtNetworkRequest::NetworkDownloadTask::State::Paused)
            canStart = true;
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Running)
        {
            canPause = true;
            canCancel = true;
        }
    }

    ui->btnStart->setEnabled(canStart);
    ui->btnPause->setEnabled(canPause);
    ui->btnCancel->setEnabled(canCancel);
    ui->btnDelete->setEnabled(hasSelection);
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

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskFileNameChanged(const QString &taskId, const QString &fileName)
{
    m_taskModel->updateTaskFileName(taskId, fileName);
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
        showNotification(QStringLiteral("下载失败：%1").arg(task.fileName), "error", 5000);
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onTaskCompleted(const QString &taskId, bool success)
{
    if (success)
    {
        QtNetworkRequest::NetworkDownloadTask task = m_downloadManager->getDownloadTask(taskId);
        // The download manager may have renamed the file based on
        // Content-Disposition / final URL. Refresh the model's fileName
        // directly here as a reliable fallback in case the
        // taskFileNameChanged signal didn't propagate.
        m_taskModel->updateTaskFileName(taskId, task.fileName);
        m_taskModel->updateTaskTotalSpeed(taskId);
        showNotification(QStringLiteral("'%1' 下载完成").arg(task.fileName), "success", 4000);
    }
    updateUI();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onDownloadSpeedChanged(qint64 totalSpeed)
{
    QString speedText;
    if (totalSpeed <= 0)
    {
        speedText = QStringLiteral("--");
    }
    else if (totalSpeed < 1024)
    {
        speedText = QString("%1 B/s").arg(totalSpeed);
    }
    else if (totalSpeed < 1024 * 1024)
    {
        speedText = QString("%1 KB/s").arg(totalSpeed / 1024);
    }
    else
    {
        double speedMB = static_cast<double>(totalSpeed) / (1024.0 * 1024.0);
        speedText = QString("%1 MB/s").arg(speedMB, 0, 'f', 1);
    }

    ui->labelSpeed->setText(speedText);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActiveDownloadsChanged(int count)
{
    Q_UNUSED(count);
    updateUI();
    updateGlobalSpeed();
    updateTimeRemaining();
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

// ── Theme handling ───────────────────────────────────────────────────────────
void QtNetworkRequest::NetworkDownloaderMainWindow::onActionThemeLight()
{
    if (m_theme) m_theme->setMode(ThemeManager::Mode::Light);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionThemeDark()
{
    if (m_theme) m_theme->setMode(ThemeManager::Mode::Dark);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionThemeSystem()
{
    if (m_theme) m_theme->setMode(ThemeManager::Mode::System);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::onActionToggleTheme()
{
    if (m_theme) m_theme->toggle();
}

void QtNetworkRequest::NetworkDownloaderMainWindow::syncThemeActionGroup()
{
    if (!m_theme)
        return;
    const auto mode = m_theme->mode();
    QAction *checked = nullptr;
    if (mode == ThemeManager::Mode::Light)
        checked = ui->actionThemeLight;
    else if (mode == ThemeManager::Mode::Dark)
        checked = ui->actionThemeDark;
    else
        checked = ui->actionThemeSystem;
    if (checked)
        checked->setChecked(true);
}

void QtNetworkRequest::NetworkDownloaderMainWindow::updateUI()
{
    int runningCount = m_taskModel->getRunningTaskCount();
    int totalCount = m_taskModel->rowCount();

    int completedCount = 0;
    const QVector<QtNetworkRequest::NetworkDownloadTask> tasks = m_taskModel->getAllTasks();
    for (const QtNetworkRequest::NetworkDownloadTask &task : tasks)
    {
        if (task.state == QtNetworkRequest::NetworkDownloadTask::State::Completed)
            completedCount++;
    }

    // Task-count badge in the task panel header (prototype: task-count-badge)
    ui->lblTaskCount->setText(QStringLiteral("%1 个任务").arg(totalCount));

    // Status bar stats (prototype: 任务 / 运行中 / 已完成)
    QString statusText = QStringLiteral("任务: %1 个 · 运行中: %2 个 · 已完成: %3 个")
                             .arg(totalCount).arg(runningCount).arg(completedCount);
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
            timeText = QString("%1s").arg(remainingSeconds);
        }
        else if (remainingSeconds < 3600)
        {
            int minutes = remainingSeconds / 60;
            int seconds = remainingSeconds % 60;
            timeText = QString("%1m %2s").arg(minutes).arg(seconds);
        }
        else
        {
            int hours = remainingSeconds / 3600;
            int minutes = (remainingSeconds % 3600) / 60;
            timeText = QString("%1h %2m").arg(hours).arg(minutes);
        }

        ui->labelTime->setText(timeText);
    }
    else
    {
        ui->labelTime->setText("--");
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::showSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("设置");
    dialog.setMinimumWidth(440);
    // The dialog inherits the main-window stylesheet which covers QDialog,
    // QGroupBox, QLineEdit, QSpinBox, QPushButton, and QDialogButtonBox with
    // the VSCode-inspired dark theme, ensuring visual consistency.

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 14, 16, 14);

    // ---------- Download Directory ----------
    auto *dirGroup = new QGroupBox("下载目录");
    auto *dirLayout = new QFormLayout(dirGroup);
    dirLayout->setSpacing(5);
    dirLayout->setContentsMargins(12, 10, 12, 10);
    dirLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *dirEdit = new QLineEdit(m_downloadManager->getDownloadDirectory());
    auto *dirButton = new QPushButton("浏览...");
    dirButton->setObjectName("btnBrowse");

    auto *dirRow = new QHBoxLayout();
    dirRow->addWidget(dirEdit);
    dirRow->addWidget(dirButton);
    dirLayout->addRow("路径:", dirRow);

    mainLayout->addWidget(dirGroup);

    // ---------- Download Options ----------
    auto *dlGroup = new QGroupBox("下载选项");
    auto *dlLayout = new QFormLayout(dlGroup);
    dlLayout->setSpacing(5);
    dlLayout->setContentsMargins(12, 10, 12, 10);
    dlLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *threadSpinBox = new QSpinBox();
    threadSpinBox->setRange(1, 64);
    threadSpinBox->setValue(m_downloadManager->getMaxThreads());
    threadSpinBox->setToolTip("每个下载任务使用的线程数（1-64）");

    auto *concurrentSpinBox = new QSpinBox();
    concurrentSpinBox->setRange(1, 20);
    concurrentSpinBox->setValue(m_downloadManager->getMaxConcurrentDownloads());
    concurrentSpinBox->setToolTip("同时运行的最大下载数（1-20）");

    dlLayout->addRow("最大线程数:", threadSpinBox);
    dlLayout->addRow("最大并发数:", concurrentSpinBox);

    mainLayout->addWidget(dlGroup);

    // -- Buttons --
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Connections
    connect(dirButton, &QPushButton::clicked, [&]()
            {
        QString dir = QFileDialog::getExistingDirectory(&dialog, "选择下载目录", dirEdit->text());
        if (!dir.isEmpty()) {
            dirEdit->setText(dir);
        } });

    if (dialog.exec() == QDialog::Accepted)
    {
        m_downloadManager->setDownloadDirectory(dirEdit->text());
        m_downloadManager->setMaxThreads(threadSpinBox->value());
        m_downloadManager->setMaxConcurrentDownloads(concurrentSpinBox->value());
        if (m_labelDownloadDir)
        {
            m_labelDownloadDir->setText(QStringLiteral("下载目录: %1").arg(m_downloadManager->getDownloadDirectory()));
        }
    }
}

void QtNetworkRequest::NetworkDownloaderMainWindow::showAboutDialog()
{
    QMessageBox::about(this, "关于 Qt Downloader",
                       "<div style='font-family: \"Segoe UI\", Arial, sans-serif;'>"
                       "<h3>Qt Downloader v1.0</h3>"
                       "<p>基于 Qt 构建的现代多线程下载管理器</p>"
                       "<h4>🚀 功能特性</h4>"
                       "<ul>"
                       "<li>多线程下载（最多 64 线程）</li>"
                       "<li>批量 URL 处理</li>"
                       "<li>实时进度跟踪</li>"
                       "<li>暂停 / 继续功能</li>"
                       "<li>速度监控与统计</li>"
                       "<li>明暗双主题 UI</li>"
                       "</ul>"
                       "<h4>⚙️ 技术细节</h4>"
                       "<ul>"
                       "<li>基于 Qt5 框架</li>"
                       "<li>使用 QNetworkRequest 库</li>"
                       "<li>支持 HTTP/HTTPS/FTP 协议</li>"
                       "<li>跨平台兼容</li>"
                       "</ul>"
                       "<h4>📄 许可证</h4>"
                       "<p>LGPL v3.0</p>"
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
                                    "   background-color: %2;"
                                    "   color: %3;"
                                    "   border: 1px solid %4;"
                                    "   border-left: 3px solid %1;"
                                    "   border-radius: 7px;"
                                    "   padding: 10px 18px;"
                                    "   font-family: 'Segoe UI', Arial, sans-serif;"
                                    "   font-size: 13px;"
                                    "   font-weight: 500;"
                                    "}")
                                    .arg(getNotificationColor(type))
                                    .arg(getNotificationBackground())
                                    .arg(getNotificationTextColor())
                                    .arg(getNotificationBorderColor()));

    // Calculate appropriate size based on content
    QFontMetrics fm(notification->font());
    int maxWidth = qMin(500, this->width() - 60); // Max 500px or window width minus margins
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    int textWidth = fm.horizontalAdvance(message);
#else
    int textWidth = fm.width(message);
#endif

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
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    if (type == "success")
    {
        return dark ? "#34d399" : "#2d8a56";
    }
    else if (type == "warning")
    {
        return dark ? "#fbbf24" : "#d97706";
    }
    else if (type == "error")
    {
        return dark ? "#f87171" : "#dc2626";
    }
    else
    {
        return dark ? "#7b8cff" : "#4361ee";
    }
}

QString QtNetworkRequest::NetworkDownloaderMainWindow::getNotificationBackground() const
{
    return palette().color(QPalette::Window).lightness() < 128 ? "#1a1a26" : "#ffffff";
}

QString QtNetworkRequest::NetworkDownloaderMainWindow::getNotificationTextColor() const
{
    return palette().color(QPalette::Window).lightness() < 128 ? "#e4e3ed" : "#1a1a24";
}

QString QtNetworkRequest::NetworkDownloaderMainWindow::getNotificationBorderColor() const
{
    return palette().color(QPalette::Window).lightness() < 128 ? "#363648" : "#dbd9d2";
}