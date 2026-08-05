#include <QCoreApplication>
#include <QtTest/QtTest>
#include "test_requestbuilder.h"
#include "test_bodytype.h"
#include "test_environment.h"
#include "networkrequestmanager.h"

// Dedicated entry point for the pure (no-network) RequestContextBuilder unit
// tests. Kept in its own executable so the fluent-API contract tests run
// independently of the networking test suite.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QtNetworkRequest::NetworkRequestManager::initialize();

    int status = 0;
    TestRequestBuilder testRequestBuilder;
    status |= QTest::qExec(&testRequestBuilder, argc, argv);
    BodyTypeTests bodyTypeTests;
    status |= QTest::qExec(&bodyTypeTests, argc, argv);
    EnvironmentTests environmentTests;
    status |= QTest::qExec(&environmentTests, argc, argv);

    QtNetworkRequest::NetworkRequestManager::unInitialize();
    return status;
}
