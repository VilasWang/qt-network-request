#include <QCoreApplication>
#include <QtTest/QtTest>
#include <QSslSocket>
#include "test_networkrequest.h"
#include "networkrequestdefs.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "SSL supported:" << QSslSocket::supportsSsl();
    qDebug() << "SSL library build version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "SSL library runtime version:" << QSslSocket::sslLibraryVersionNumber();
    qDebug() << "SSL library runtime version string:" << QSslSocket::sslLibraryVersionString();

    // Register meta types for signal/slot connections
    qRegisterMetaType<QSharedPointer<QtNetworkRequest::ResponseResult>>("QSharedPointer<QtNetworkRequest::ResponseResult>");

    // Create test instance
    TestNetworkRequest testNetworkRequest;

    // Execute test cases
    return QTest::qExec(&testNetworkRequest, argc, argv);
}