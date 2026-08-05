#include <QCoreApplication>
#include <QtTest/QtTest>
#include "test_collection.h"
#include "test_postman.h"
#include "networkrequestmanager.h"

// Entry point for the pure (no-network) Collection model + Postman v2.1 converter tests.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QtNetworkRequest::NetworkRequestManager::initialize();

    int status = 0;
    CollectionTests collectionTests;
    status |= QTest::qExec(&collectionTests, argc, argv);
    PostmanTests postmanTests;
    status |= QTest::qExec(&postmanTests, argc, argv);

    QtNetworkRequest::NetworkRequestManager::unInitialize();
    return status;
}
