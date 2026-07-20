#include <QApplication>
#include <QtTest/QtTest>
#include "test_qtrequester.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("UiTests");
    app.setOrganizationName("QtNetworkRequest");

    TestQtRequester tester;
    return QTest::qExec(&tester, argc, argv);
}
