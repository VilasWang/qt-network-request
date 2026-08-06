#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

// ================================================================
// ThemeManager — runtime Light / Dark / System theme controller.
//
// Loads the "Precision Workshop" QSS themes (embedded as Qt resources
// at ":/styles/precision-light.qss" and ":/styles/precision-dark.qss")
// onto qApp so they cascade over the whole application. The persisted
// choice lives in QSettings("QtNetworkRequest","Theme") under the key
// "themeMode", so it survives restarts and is shared by the sample
// apps and the GUI test harnesses.
//
// System mode resolves the OS preference from the current GUI palette:
// a dark window background ⇒ dark. This is Qt 5.15-safe and needs no
// platform-specific API.
// ================================================================

#include <QObject>
#include <QSettings>
#include <QGuiApplication>
#include <QPalette>
#include <QFile>
#include <QApplication>
#include <QColor>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum class Mode
    {
        Light,
        Dark,
        System
    };
    Q_ENUM(Mode)

    explicit ThemeManager(QObject *parent = nullptr)
        : QObject(parent)
        , m_mode(loadMode())
    {
    }

    // Apply (and persist) the requested mode. Resolves System to a concrete
    // theme, sets qApp's stylesheet, and emits modeChanged().
    void setMode(Mode mode)
    {
        if (mode == m_mode && !qApp->styleSheet().isEmpty())
            return;
        m_mode = mode;
        saveMode(mode);
        apply();
        emit modeChanged(m_mode);
    }

    Mode mode() const { return m_mode; }

    // Resolves System → concrete light/dark flag.
    bool isDark() const
    {
        if (m_mode == Mode::Light)
            return false;
        if (m_mode == Mode::Dark)
            return true;
        return systemIsDark();
    }

    // Toolbar-toggle helper: flips between Light and Dark (resolving System
    // to its concrete value first so the toggle always changes appearance).
    void toggle()
    {
        setMode(isDark() ? Mode::Light : Mode::Dark);
    }

    // Convenience: apply whatever is stored in QSettings to qApp, callable
    // from main() before any widget is shown so the first paint is themed.
    static void applyStoredMode()
    {
        const Mode m = loadMode();
        qApp->setStyleSheet(loadStylesheet(m));
    }

    // Theme accent color for inline-styled text (e.g. response body) so it
    // stays legible without a QSS rule. Light → indigo, Dark → periwinkle,
    // matching the design tokens in the QSS files.
    QColor accent() const
    {
        return isDark() ? QColor(0x7b, 0x8c, 0xff) : QColor(0x43, 0x61, 0xee);
    }

signals:
    void modeChanged(Mode mode);

private:
    Mode m_mode;

    static bool systemIsDark()
    {
        if (!qApp)
            return false;
        // A dark window-background palette ⇒ dark mode. lightness() is in
        // [0,255]; 128 is the conventional midpoint threshold.
        const int lightness = qApp->palette().color(QPalette::Window).lightness();
        return lightness < 128;
    }

    static QString themeResourcePath(Mode resolvedMode)
    {
        // resolvedMode must already be concrete (Light/Dark), not System.
        if (resolvedMode == Mode::Dark)
            return QStringLiteral(":/styles/precision-dark.qss");
        return QStringLiteral(":/styles/precision-light.qss");
    }

    static QString loadStylesheet(Mode mode)
    {
        const Mode resolved = (mode == Mode::System)
                                  ? (systemIsDark() ? Mode::Dark : Mode::Light)
                                  : mode;
        QFile f(themeResourcePath(resolved));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(f.readAll());
    }

    void apply()
    {
        if (qApp)
            qApp->setStyleSheet(loadStylesheet(m_mode));
    }

    static Mode loadMode()
    {
        // Test/automation override: QT_PRECISION_THEME=light|dark pins the
        // theme without touching persisted settings.
        const QByteArray overrideMode = qgetenv("QT_PRECISION_THEME");
        if (overrideMode == "light") { s_envOverride = true; return Mode::Light; }
        if (overrideMode == "dark")  { s_envOverride = true; return Mode::Dark; }
        QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
        const int v = s.value(QStringLiteral("themeMode"),
                              static_cast<int>(Mode::System)).toInt();
        if (v == static_cast<int>(Mode::Light))
            return Mode::Light;
        if (v == static_cast<int>(Mode::Dark))
            return Mode::Dark;
        return Mode::System;
    }

    static void saveMode(Mode mode)
    {
        if (s_envOverride)
            return; // Never persist an automation-forced theme.
        QSettings s(QStringLiteral("QtNetworkRequest"), QStringLiteral("Theme"));
        s.setValue(QStringLiteral("themeMode"), static_cast<int>(mode));
    }

    static inline bool s_envOverride = false;
};

#endif // THEMEMANAGER_H
