#ifndef TEST_DOWNLOADERUI_H
#define TEST_DOWNLOADERUI_H

#include <QObject>
#include <QtTest/QtTest>

// DownloaderUiTests — GUI rendering & theme-switch tests for the QtDownloader
// sample. The existing DownloaderTests suite is non-GUI (QCoreApplication) and
// cannot exercise rendering, so this suite uses QApplication and constructs the
// full NetworkDownloaderMainWindow.
class TestDownloaderUi : public QObject
{
    Q_OBJECT

private slots:
    void init();

    // --- rendering under each theme ---
    void testDownloaderLightRenders();
    void testDownloaderDarkRenders();
    void testDownloaderWidgetsHaveThemedPalette();

    // --- View menu exists and works ---
    void testViewMenuExists();
    void testThemeActionsCheckableAndExclusive();
    void testActionLightSwitchesTheme();
    void testActionDarkSwitchesTheme();
    void testActionToggleFlipsTheme();

    // --- interaction ---
    void testDownloaderThemeButtonToggle();
};

#endif // TEST_DOWNLOADERUI_H
