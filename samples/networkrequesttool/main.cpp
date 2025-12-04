#include <QtWidgets/QApplication>
#include <QDir>
#include <QDebug>
#include "networkrequesttool.h"

int main(int argc, char *argv[])
{
    // Enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon("resources/networktool.ico"));
    QtNetworkRequest::NetworkRequestTool w;
    w.show();
    return a.exec();
}
