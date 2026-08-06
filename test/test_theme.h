#ifndef TEST_THEME_H
#define TEST_THEME_H

#include <QObject>
#include <QtTest/QtTest>

// ThemeTests — runtime Light/Dark/System theming for the QtRequester sample.
//
// Covers three layers:
//   1. ThemeManager unit behaviour (default mode, persistence, toggle, QSS load).
//   2. QtRequester rendering under each concrete theme (palette resolves to the
//      expected background; no black-on-black; key widgets present & themed).
//   3. Theme interactions (toolbar toggle flips mode; mode survives recreate).
class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    // --- ThemeManager unit tests ---
    void testDefaultModeIsSystem();
    void testSetModeLightResolvesAndApplies();
    void testSetModeDarkResolvesAndApplies();
    void testToggleFlipsLightDark();
    void testPersistenceRoundTrip();
    void testApplyStoredModeLoadsStylesheet();
    void testAccentMatchesMode();
    void testStylesheetContainsThemeTokens();

    // --- QtRequester rendering under each theme ---
    void testRequesterLightBackground();
    void testRequesterDarkBackground();
    void testRequesterWidgetsHaveThemedPalette();
    void testRequesterThemeButtonExists();
    void testRequesterNoInlineMainWindowStylesheet();

    // --- Theme interactions ---
    void testRequesterToggleChangesStylesheet();
    void testRequesterModeSurvivesRecreate();
};

#endif // TEST_THEME_H
