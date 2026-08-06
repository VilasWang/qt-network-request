#include <QApplication>
#include "downloadermainwindow.h"
#include "thememanager.h"

int main(int argc, char *argv[])
{
    // Enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    // Set application information
    app.setApplicationName("Qt Downloader");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("QtDownloader");
    app.setOrganizationDomain("qtdownloader.com");

    // Apply the persisted theme (Light/Dark/System) before any widget is
    // shown so the very first paint is already themed.
    ThemeManager::applyStoredMode();

    // Create and show main window
    QtNetworkRequest::NetworkDownloaderMainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}