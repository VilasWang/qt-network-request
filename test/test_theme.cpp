#include <QtTest/QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QMainWindow>
#include <QSettings>
#include <QFile>
#include "test_theme.h"
#include "thememanager.h"
#include "networkrequesttool.h"

using namespace QtNetworkRequest;

namespace
{
    // Reset the persisted theme setting so each test starts from a known state.
    // ThemeManager reads/writes QSettings("QtNetworkRequest","Theme") / themeMode.
    void resetThemeSetting()
    {
        QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
        s.remove(QStringLiteral("themeMode"));
    }

    void setThemeSetting(int mode)
    {
        QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
        s.setValue(QStringLiteral("themeMode"), mode);
    }

    // Light accent = #4361ee, Dark accent = #7b8cff (design tokens in the QSS).
    bool stylesheetHasAccent(const QString &qss, bool dark)
    {
        return dark ? qss.contains(QStringLiteral("#7b8cff"), Qt::CaseInsensitive)
                    : qss.contains(QStringLiteral("#4361ee"), Qt::CaseInsensitive);
    }
}

// ---------------------------------------------------------------------------
// ThemeManager unit tests
// ---------------------------------------------------------------------------

void TestTheme::testDefaultModeIsSystem()
{
    resetThemeSetting();
    ThemeManager tm;
    QCOMPARE(tm.mode(), ThemeManager::Mode::System);
}

void TestTheme::testSetModeLightResolvesAndApplies()
{
    resetThemeSetting();
    ThemeManager tm;
    tm.setMode(ThemeManager::Mode::Light);
    QVERIFY(!tm.isDark());
    QVERIFY(!qApp->styleSheet().isEmpty());
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));

    // Persisted
    QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
    QCOMPARE(s.value("themeMode").toInt(), static_cast<int>(ThemeManager::Mode::Light));
}

void TestTheme::testSetModeDarkResolvesAndApplies()
{
    resetThemeSetting();
    ThemeManager tm;
    tm.setMode(ThemeManager::Mode::Dark);
    QVERIFY(tm.isDark());
    QVERIFY(!qApp->styleSheet().isEmpty());
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));
}

void TestTheme::testToggleFlipsLightDark()
{
    resetThemeSetting();
    ThemeManager tm;

    tm.setMode(ThemeManager::Mode::Light);
    QVERIFY(!tm.isDark());
    tm.toggle();
    QVERIFY(tm.isDark());
    QCOMPARE(tm.mode(), ThemeManager::Mode::Dark);

    tm.toggle();
    QVERIFY(!tm.isDark());
    QCOMPARE(tm.mode(), ThemeManager::Mode::Light);
}

void TestTheme::testPersistenceRoundTrip()
{
    resetThemeSetting();
    setThemeSetting(static_cast<int>(ThemeManager::Mode::Dark));

    // A freshly-constructed manager reads the persisted value.
    ThemeManager tm;
    QCOMPARE(tm.mode(), ThemeManager::Mode::Dark);

    tm.setMode(ThemeManager::Mode::Light);
    // New instance picks up the just-written Light value.
    ThemeManager tm2;
    QCOMPARE(tm2.mode(), ThemeManager::Mode::Light);
}

void TestTheme::testApplyStoredModeLoadsStylesheet()
{
    resetThemeSetting();
    qApp->setStyleSheet(QString());

    setThemeSetting(static_cast<int>(ThemeManager::Mode::Dark));
    ThemeManager::applyStoredMode();
    QVERIFY(!qApp->styleSheet().isEmpty());
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));

    setThemeSetting(static_cast<int>(ThemeManager::Mode::Light));
    ThemeManager::applyStoredMode();
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));
}

void TestTheme::testAccentMatchesMode()
{
    resetThemeSetting();
    ThemeManager tm;

    tm.setMode(ThemeManager::Mode::Light);
    QCOMPARE(tm.accent(), QColor(0x43, 0x61, 0xee));

    tm.setMode(ThemeManager::Mode::Dark);
    QCOMPARE(tm.accent(), QColor(0x7b, 0x8c, 0xff));
}

void TestTheme::testStylesheetContainsThemeTokens()
{
    resetThemeSetting();
    ThemeManager tm;
    tm.setMode(ThemeManager::Mode::Dark);
    const QString qss = qApp->styleSheet();
    // Sanity: the QSS is non-trivial and targets key controls used by the apps.
    QVERIFY(qss.contains(QStringLiteral("QMainWindow")));
    QVERIFY(qss.contains(QStringLiteral("QPushButton")));
    QVERIFY(qss.contains(QStringLiteral("QComboBox")));
    QVERIFY(qss.contains(QStringLiteral("QTableView")));
}

// ---------------------------------------------------------------------------
// QtRequester rendering under each theme
// ---------------------------------------------------------------------------

void TestTheme::testRequesterLightBackground()
{
    resetThemeSetting();
    {
        ThemeManager tm;  // apply Light
        tm.setMode(ThemeManager::Mode::Light);
    }

    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // The QSS colours the main window background; the active app stylesheet
    // must reflect the light theme.
    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), false));

    // Key controls exist and are enabled/visible.
    auto *url = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(url);
    auto *method = tool.findChild<QComboBox *>("cmb_method");
    QVERIFY(method);
    auto *send = tool.findChild<QPushButton *>("btn_send");
    QVERIFY(send);
}

void TestTheme::testRequesterDarkBackground()
{
    resetThemeSetting();
    {
        ThemeManager tm;
        tm.setMode(ThemeManager::Mode::Dark);
    }

    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    QVERIFY(stylesheetHasAccent(qApp->styleSheet(), true));

    auto *body = tool.findChild<QTextEdit *>("textEdit_body");
    QVERIFY(body);
    auto *params = tool.findChild<QTableWidget *>("table_params");
    QVERIFY(params);
    auto *headers = tool.findChild<QTableWidget *>("table_headers");
    QVERIFY(headers);
}

void TestTheme::testRequesterWidgetsHaveThemedPalette()
{
    resetThemeSetting();
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Every themed control must have a resolvable foreground colour (no default
    // black-on-black): a themed control's effective text colour is never equal
    // to its background colour, and the foreground must be explicitly set.
    const auto checkNotBlackOnBlack = [](QWidget *w) {
        if (!w)
            return;
        const QPalette p = w->palette();
        const QColor fg = p.color(QPalette::Active, QPalette::WindowText);
        const QColor bg = p.color(QPalette::Active, QPalette::Window);
        QVERIFY2(fg != bg, qPrintable(QString("fg==bg for %1").arg(w->objectName())));
    };

    checkNotBlackOnBlack(tool.findChild<QLineEdit *>("lineEdit_url"));
    checkNotBlackOnBlack(tool.findChild<QComboBox *>("cmb_method"));
    checkNotBlackOnBlack(tool.findChild<QPushButton *>("btn_send"));
}

void TestTheme::testRequesterThemeButtonExists()
{
    resetThemeSetting();
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *btnTheme = tool.findChild<QPushButton *>("btn_theme");
    QVERIFY(btnTheme);
    QVERIFY(btnTheme->isEnabled());

    const bool wasDark = tool.m_theme && tool.m_theme->isDark();
    QTest::mouseClick(btnTheme, Qt::LeftButton);
    QTest::qWait(30);
    QVERIFY(tool.m_theme);
    QVERIFY(tool.m_theme->isDark() != wasDark);
}

void TestTheme::testRequesterNoInlineMainWindowStylesheet()
{
    resetThemeSetting();
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // The legacy multi-KB embedded stylesheet was removed from the .ui; the
    // main window itself must not carry a large inline sheet that would
    // override the app-level QSS. (Theme is applied via qApp->styleSheet.)
    const QString sheet = tool.styleSheet();
    QVERIFY2(sheet.length() < 200,
             qPrintable(QString("QMainWindow carries an unexpected inline "
                                "stylesheet (%1 chars)").arg(sheet.length())));
}

// ---------------------------------------------------------------------------
// Theme interactions
// ---------------------------------------------------------------------------

void TestTheme::testRequesterToggleChangesStylesheet()
{
    resetThemeSetting();
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    tool.m_theme->setMode(ThemeManager::Mode::Light);
    const QString lightSheet = qApp->styleSheet();
    QVERIFY(stylesheetHasAccent(lightSheet, false));

    tool.m_theme->setMode(ThemeManager::Mode::Dark);
    const QString darkSheet = qApp->styleSheet();
    QVERIFY(stylesheetHasAccent(darkSheet, true));

    QVERIFY(lightSheet != darkSheet);
}

void TestTheme::testRequesterModeSurvivesRecreate()
{
    resetThemeSetting();
    {
        NetworkRequestTool tool;
        tool.show();
        QTest::qWait(30);
        tool.m_theme->setMode(ThemeManager::Mode::Dark);
    }
    // A second window must read the persisted Dark choice.
    NetworkRequestTool tool2;
    tool2.show();
    QTest::qWait(30);
    QCOMPARE(tool2.m_theme->mode(), ThemeManager::Mode::Dark);
    QVERIFY(tool2.m_theme->isDark());
}
