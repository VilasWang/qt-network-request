#include <QApplication>
#include "downloadermainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application information
    app.setApplicationName("Qt Downloader");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("QtDownloader");
    app.setOrganizationDomain("qtdownloader.com");

    // Create and show main window
    QtNetworkRequest::NetworkDownloaderMainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}