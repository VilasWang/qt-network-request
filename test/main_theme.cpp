#include <QApplication>
#include <QtTest/QtTest>
#include "test_theme.h"
#include "thememanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ThemeTests");
    app.setOrganizationName("QtNetworkRequest");

    // Tests assert on qApp->styleSheet(); start from a clean, themed state.
    ThemeManager::applyStoredMode();

    TestTheme tester;
    return QTest::qExec(&tester, argc, argv);
}
