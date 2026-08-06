#include <QtWidgets/QApplication>
#include <QDir>
#include <QDebug>
#include "networkrequesttool.h"
#include "thememanager.h"

int main(int argc, char *argv[])
{
    // Enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon("resources/networktool.ico"));

    // Apply the persisted theme (Light/Dark/System) before any widget is
    // shown so the very first paint is already themed.
    ThemeManager::applyStoredMode();

    QtNetworkRequest::NetworkRequestTool w;
    w.show();
    return a.exec();
}
