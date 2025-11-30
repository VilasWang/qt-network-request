#ifndef TEST_NETWORKREQUEST_H
#define TEST_NETWORKREQUEST_H

#include <QObject>
#include <QtTest/QtTest>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <memory>
#include "networkrequestdefs.h"
#include "networkrequestmanager.h"
#include "networkreply.h"

using namespace QtNetworkRequest;

class TestNetworkRequest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testGetRequest();
    void testPostRequest();
    void testPostFormDataRequest();
    void testPutRequest();
    void testDeleteRequest();
    void testHeadRequest();
    void testRequestHeaders();
    void testContentType();

private:
    bool waitForFinished(std::shared_ptr<NetworkReply> reply, int timeoutMs = 10000);
};

#endif // TEST_NETWORKREQUEST_H