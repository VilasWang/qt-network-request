#ifndef NETWORKDOWNLOADERMAINWINDOW_H
#define NETWORKDOWNLOADERMAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QLabel>
#include <QResizeEvent>
#include <QFontMetrics>
#include "downloadtaskmodel.h"
#include "downloadmanager.h"

namespace Ui
{
    class DownloaderMainWindow;
}

namespace QtNetworkRequest
{

class NetworkDownloaderMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit NetworkDownloaderMainWindow(QWidget *parent = nullptr);
    ~NetworkDownloaderMainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAddTasksClicked();
    void onStartClicked();
    void onCancelClicked();
    void onDeleteClicked();
    void onTaskSelectionChanged();
    void onTaskAdded(const NetworkDownloadTask &task);
    void onTaskProgress(const QString &taskId, qint64 downloaded, qint64 total, qint64 speed);
    void onTaskElapsedTimeChanged(const QString &taskId, qint64 elapsedMillis);
    void onTaskStateChanged(const QString &taskId, NetworkDownloadTask::State state, const QString &error = QString());
    void onTaskCompleted(const QString &taskId, bool success);
    void onDownloadSpeedChanged(qint64 totalSpeed);
    void onActiveDownloadsChanged(int count);
    void onActionSettings();
    void onActionAbout();
    void onActionExit();

private:
    Ui::DownloaderMainWindow *ui;
    NetworkDownloadTaskModel *m_taskModel;
    NetworkDownloadManager *m_downloadManager;
    QSettings m_settings;

    // Notification system
    QList<QLabel*> m_notifications;
    QTimer m_notificationTimer;
    int m_notificationYOffset;

    void setupConnections();
    void updateUI();
    void updateGlobalSpeed();
    void updateTimeRemaining();
    void showSettingsDialog();
    void showAboutDialog();
    void saveGeometrySettings();
    void loadGeometrySettings();

    // Non-modal notification system
    void showNotification(const QString &message, const QString &type = "info", int duration = 3000);
    void hideNotification();
    void clearNotifications();
    QString getNotificationColor(const QString &type);

private Q_SLOTS:
    void onNotificationTimeout();
};

} // namespace QtNetworkRequest

#endif // NETWORKDOWNLOADERMAINWINDOW_H
