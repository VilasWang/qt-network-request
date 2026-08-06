#include <QApplication>
#include <QtTest/QtTest>
#include "test_downloaderui.h"
#include "thememanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("DownloaderUiTests");
    app.setOrganizationName("QtNetworkRequest");

    // Start from a clean, themed state (tests assert on qApp->styleSheet()).
    ThemeManager::applyStoredMode();

    TestDownloaderUi tester;
    return QTest::qExec(&tester, argc, argv);
}
