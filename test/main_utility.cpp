#include <QCoreApplication>
#include <QtTest/QtTest>
#include "test_utility.h"

// Dedicated entry point for the pure (no-network) NetworkRequestUtils tests.
// Kept in its own executable so the filesystem/utility contract tests run
// independently of the networking test suite.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TestUtility testUtility;
    return QTest::qExec(&testUtility, argc, argv);
}
