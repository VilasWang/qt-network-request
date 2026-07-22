#ifndef TEST_NETWORKREQUEST_H
#define TEST_NETWORKREQUEST_H

#include <QObject>
#include <QtTest/QtTest>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <memory>
#include "requestcontext.h"
#include "responseresult.h"
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
    void testGlobalProxyConfig();
    void testRequestProxyConfig();
    void testRetryOnFailure();
    void testSingleDownload();
    void testMTDownload();
    void testFileUpload();
    void testPersistentCookieJar();
    void testRequestPriority();

    // P0/P1 regression tests
    void testStopRunningRequest();
    void testStopBatchRequest();
    void testStopAllRequests();
    void testRapidCancelStress();

    // Timeout mechanism tests
    void testTotalTimeout();
    void testIdleTimeout();

private:
    bool waitForFinished(std::shared_ptr<NetworkReply> reply, int timeoutMs = 10000);
};

#endif // TEST_NETWORKREQUEST_H