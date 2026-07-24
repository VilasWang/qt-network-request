#include <QCoreApplication>
#include <QtTest/QtTest>
#include "test_requestbuilder.h"

// Dedicated entry point for the pure (no-network) RequestContextBuilder unit
// tests. Kept in its own executable so the fluent-API contract tests run
// independently of the networking test suite.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TestRequestBuilder testRequestBuilder;
    return QTest::qExec(&testRequestBuilder, argc, argv);
}
