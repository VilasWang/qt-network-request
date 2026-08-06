#include <QtTest/QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include "test_downloaderui.h"
#include "thememanager.h"
#include "downloadermainwindow.h"

using namespace QtNetworkRequest;

namespace
{
    void resetThemeSetting()
    {
        QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
        s.remove(QStringLiteral("themeMode"));
    }

    bool stylesheetHasAccent(const QString &qss, bool dark)
    {
        return dark ? qss.contains(QStringLiteral("#7b8cff"), Qt::CaseInsensitive)
                    : qss.contains(QStringLiteral("#4361ee"), Qt::CaseInsensitive);
    }
}

void TestDownloaderUi::init()
{
    resetThemeSetting();
}

// ---------------------------------------------------------------------------
// Rendering under each theme
// ---------------------------------------------------------------------------

void TestDownloaderUi::testDownloaderLightRenders()
{
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Light);
    }

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));

    auto *table = w.findChild<QTableView *>("tableViewTasks");
    QVERIFY(table);
    auto *speed = w.findChild<QLabel *>("labelSpeed");
    QVERIFY(speed);
    auto *time = w.findChild<QLabel *>("labelTime");
    QVERIFY(time);
}

void TestDownloaderUi::testDownloaderDarkRenders()
{
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Dark);
    }

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));

    auto *table = w.findChild<QTableView *>("tableViewTasks");
    QVERIFY(table);
}

void TestDownloaderUi::testDownloaderWidgetsHaveThemedPalette()
{
    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    const auto checkNotBlackOnBlack = [](QWidget *wdg) {
        if (!wdg)
            return;
        const QPalette p = wdg->palette();
        const QColor fg = p.color(QPalette::Active, QPalette::WindowText);
        const QColor bg = p.color(QPalette::Active, QPalette::Window);
        QVERIFY2(fg != bg, qPrintable(QString("fg==bg for %1").arg(wdg->objectName())));
    };

    checkNotBlackOnBlack(w.findChild<QTableView *>("tableViewTasks"));
    checkNotBlackOnBlack(w.findChild<QLabel *>("labelSpeed"));
    checkNotBlackOnBlack(w.findChild<QLabel *>("labelTime"));
}

// ---------------------------------------------------------------------------
// View menu exists and works
// ---------------------------------------------------------------------------

void TestDownloaderUi::testViewMenuExists()
{
    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *menuView = w.findChild<QMenu *>("menuView");
    QVERIFY(menuView);
}

void TestDownloaderUi::testThemeActionsCheckableAndExclusive()
{
    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *light = w.findChild<QAction *>("actionThemeLight");
    auto *dark = w.findChild<QAction *>("actionThemeDark");
    auto *system = w.findChild<QAction *>("actionThemeSystem");
    QVERIFY(light && dark && system);

    QVERIFY(light->isCheckable());
    QVERIFY(dark->isCheckable());
    QVERIFY(system->isCheckable());

    // The three share an exclusive group, so exactly one is checked at a time.
    auto *group = light->actionGroup();
    QVERIFY(group);
    QVERIFY(group->isExclusive());
    const int checkedCount = (light->isChecked() ? 1 : 0)
                           + (dark->isChecked() ? 1 : 0)
                           + (system->isChecked() ? 1 : 0);
    QCOMPARE(checkedCount, 1);
}

void TestDownloaderUi::testActionLightSwitchesTheme()
{
    // Start from Dark (persisted) so the switch to Light is observable through
    // the stylesheet and action checked-state — no private member access needed.
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Dark);
    }
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *light = w.findChild<QAction *>("actionThemeLight");
    QVERIFY(light);
    light->trigger();
    QTest::qWait(30);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));
    QVERIFY(light->isChecked());
}

void TestDownloaderUi::testActionDarkSwitchesTheme()
{
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Light);
    }

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *dark = w.findChild<QAction *>("actionThemeDark");
    QVERIFY(dark);
    dark->trigger();
    QTest::qWait(30);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));
    QVERIFY(dark->isChecked());
}

void TestDownloaderUi::testActionToggleFlipsTheme()
{
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Light);
    }
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *toggle = w.findChild<QAction *>("actionToggleTheme");
    QVERIFY(toggle);
    toggle->trigger();
    QTest::qWait(30);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void TestDownloaderUi::testDownloaderThemeButtonToggle()
{
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Light);
    }
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));

    NetworkDownloaderMainWindow w;
    w.show();
    QTest::qWait(50);

    auto *btn = w.findChild<QPushButton *>("btn_theme");
    QVERIFY(btn);
    QTest::mouseClick(btn, Qt::LeftButton);
    QTest::qWait(30);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));
}
