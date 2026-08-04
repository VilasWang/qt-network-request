#include <QtTest/QtTest>
#include "test_auth.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    AuthTests tc;
    return QTest::qExec(&tc, argc, argv);
}
