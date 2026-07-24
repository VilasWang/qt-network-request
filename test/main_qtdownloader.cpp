#include <QCoreApplication>
#include <QtTest/QtTest>
#include "test_qtdownloader.h"

// Dedicated entry point for the NetworkDownloader sample's NetworkDownloadTask
// value-type tests. The struct is header-only with deterministic helpers, so
// this suite needs no networking, no GUI and links only against Qt.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TestQtDownloader testQtDownloader;
    return QTest::qExec(&testQtDownloader, argc, argv);
}
