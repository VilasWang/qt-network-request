#include "test_networkrequest.h"
#include "httptestserver.h"
#include <QTimer>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QThread>
#include <QHttpPart>
#include <QObject>
#include <QSharedPointer>
#include <QElapsedTimer>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

using namespace QtNetworkRequest;

static HttpTestServer *s_server = nullptr;

void TestNetworkRequest::initTestCase()
{
    qRegisterMetaType<QSharedPointer<QtNetworkRequest::ResponseResult>>("QSharedPointer<QtNetworkRequest::ResponseResult>");
    NetworkRequestManager::initialize();
    QVERIFY(NetworkRequestManager::isInitialized());

    // Start local mock HTTP server
    s_server = new HttpTestServer(this);
    QVERIFY2(s_server->start(), "Failed to start HttpTestServer");
    qDebug() << "Test server on" << qPrintable(s_server->baseUrl());
}

void TestNetworkRequest::cleanupTestCase()
{
    s_server->stop();
    NetworkRequestManager::unInitialize();
    QVERIFY(!NetworkRequestManager::isInitialized());
}

bool TestNetworkRequest::waitForFinished(std::shared_ptr<NetworkReply> reply, int timeoutMs)
{
    QSignalSpy spy(reply.get(), &NetworkReply::requestFinished);
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(timeoutMs);

    while (timer.isActive() && spy.isEmpty())
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    return !spy.isEmpty();
}

void TestNetworkRequest::testGetRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?test1=1&test2=2";
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testPostRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/post";
    req->type = RequestType::Post;
    req->body = QString("{\"test\": \"data\"}");
    req->headers.insert("Content-Type", "application/json");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testPostFormDataRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/post";
    req->type = RequestType::Post;
    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->useFormData = true;
    req->uploadConfig->kvPairs.insert("key", "value");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testPutRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/put";
    req->type = RequestType::Put;
    req->headers.insert("Content-Type", "application/json");
    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->usePutMethod = true;
    req->uploadConfig->useStream = true;
    req->uploadConfig->data = QString("{\"test\": \"put_data\"}").toUtf8();

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testDeleteRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/delete";
    req->type = RequestType::Delete;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testHeadRequest()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get";
    req->type = RequestType::Head;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->headers.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testRequestHeaders()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/headers";
    req->type = RequestType::Get;
    req->headers.insert("X-Custom-Header", "CustomValue");
    req->headers.insert("Accept", "application/json");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testContentType()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/post";
    req->type = RequestType::Post;
    req->body = QString("key1=value1&key2=value2");
    req->headers.insert("Content-Type", "application/x-www-form-urlencoded");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testGlobalProxyConfig()
{
    ProxyConfig config;
    config.enabled = true;
    config.type = QNetworkProxy::HttpProxy;
    config.host = QString("10.0.0.1");
    config.port = 3128;
    config.user = QString("proxyuser");
    config.password = QString("proxypass");

    NetworkRequestManager::setGlobalProxy(config);

    const ProxyConfig &retrieved = NetworkRequestManager::globalProxy();
    QVERIFY(retrieved.enabled);
    QCOMPARE(retrieved.host, QString("10.0.0.1"));
    QCOMPARE(retrieved.port, 3128);
    QCOMPARE(retrieved.user, QString("proxyuser"));
    QCOMPARE(retrieved.password, QString("proxypass"));

    QNetworkProxy qproxy = retrieved.toQNetworkProxy();
    QCOMPARE(qproxy.type(), QNetworkProxy::HttpProxy);
    QCOMPARE(qproxy.hostName(), QString("10.0.0.1"));
    QCOMPARE(qproxy.port(), 3128);
    QCOMPARE(qproxy.user(), QString("proxyuser"));
    QCOMPARE(qproxy.password(), QString("proxypass"));

    ProxyConfig configNoAuth;
    configNoAuth.enabled = true;
    configNoAuth.host = QString("10.0.0.2");
    configNoAuth.port = 8888;
    QNetworkProxy qproxyNoAuth = configNoAuth.toQNetworkProxy();
    QCOMPARE(qproxyNoAuth.hostName(), QString("10.0.0.2"));
    QCOMPARE(qproxyNoAuth.port(), 8888);
    QVERIFY(qproxyNoAuth.user().isEmpty());

    ProxyConfig empty;
    NetworkRequestManager::setGlobalProxy(empty);
}

void TestNetworkRequest::testRequestProxyConfig()
{
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get";
    req->type = RequestType::Get;
    req->proxyConfig = std::make_unique<ProxyConfig>();
    req->proxyConfig->enabled = true;
    req->proxyConfig->type = QNetworkProxy::HttpProxy;
    req->proxyConfig->host = QString("127.0.0.1");
    req->proxyConfig->port = 1;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(!rsp->success);
                         });
    }

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
}

void TestNetworkRequest::testRetryOnFailure()
{
    s_server->setFailPath("/retry-test", 2);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/retry-test";
    req->type = RequestType::Get;
    req->behavior.retryOnFailed = true;
    req->behavior.maxRetryCount = 3;
    req->behavior.retryDelayMs = 50;
    req->behavior.transferTimeout = 3000;

    QElapsedTimer timer;
    timer.start();

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                         });
    }

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);
    QVERIFY(s_server->allRetriesPassed());

    qint64 elapsed = timer.elapsed();
    QVERIFY(elapsed >= 50);

    std::unique_ptr<RequestContext> reqNoRetry = std::make_unique<RequestContext>();
    reqNoRetry->url = QString("http://127.0.0.1:1/");
    reqNoRetry->type = RequestType::Get;
    reqNoRetry->behavior.retryOnFailed = false;
    reqNoRetry->behavior.transferTimeout = 3000;

    QElapsedTimer timerNoRetry;
    timerNoRetry.start();

    std::shared_ptr<NetworkReply> replyNoRetry = NetworkRequestManager::globalInstance()->postRequest(std::move(reqNoRetry));
    QVERIFY(replyNoRetry != nullptr);

    bool calledNoRetry = false;
    if (replyNoRetry)
    {
        QObject::connect(replyNoRetry.get(), &NetworkReply::requestFinished,
                         [&calledNoRetry](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             calledNoRetry = true;
                             QVERIFY(rsp);
                             QVERIFY(!rsp->success);
                         });
    }

    QVERIFY(waitForFinished(replyNoRetry, 30000));
    QVERIFY(calledNoRetry);

    qint64 elapsedNoRetry = timerNoRetry.elapsed();
    qDebug() << "Retry elapsed:" << elapsed << "ms, No-retry elapsed:" << elapsedNoRetry << "ms";
}

void TestNetworkRequest::testSingleDownload()
{
    QString tmpDir = QDir::tempPath();
    QString savePath = tmpDir + "/qt_test_download_" + QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(savePath);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/bytes/2048";
    req->type = RequestType::Download;
    req->downloadConfig = std::make_unique<DownloadConfig>();
    req->downloadConfig->threadCount = 1;
    req->downloadConfig->saveDir = tmpDir;
    req->downloadConfig->saveFileName = QFileInfo(savePath).fileName();
    req->downloadConfig->overwriteFile = true;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                         });
    }

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);

    QFileInfo fi(savePath);
    QVERIFY2(fi.exists(), "Downloaded file should exist");
    QCOMPARE(fi.size(), 2048);
    QFile::remove(savePath);
}

void TestNetworkRequest::testMTDownload()
{
    QString tmpDir = QDir::tempPath();
    QString savePath = tmpDir + "/qt_test_mtdownload_" + QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(savePath);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/bytes/4096";
    req->type = RequestType::Download;
    req->downloadConfig = std::make_unique<DownloadConfig>();
    req->downloadConfig->threadCount = 2;
    req->downloadConfig->saveDir = tmpDir;
    req->downloadConfig->saveFileName = QFileInfo(savePath).fileName();
    req->downloadConfig->overwriteFile = true;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                         });
    }

    QVERIFY(waitForFinished(reply, 60000));
    QVERIFY(called);

    QFileInfo fi(savePath);
    QVERIFY2(fi.exists(), "Multi-thread downloaded file should exist");
    QCOMPARE(fi.size(), 4096);
    QFile::remove(savePath);
}

void TestNetworkRequest::testFileUpload()
{
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QByteArray uploadContent("QtNetworkRequest upload test data - 1234567890");
    tmpFile.write(uploadContent);
    tmpFile.flush();
    QString filePath = tmpFile.fileName();
    tmpFile.close();

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/put";
    req->type = RequestType::Put;
    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->usePutMethod = true;
    req->uploadConfig->filePath = filePath;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [&called, uploadContent](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             called = true;
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(rsp->body.contains(uploadContent));
                         });
    }

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);
}

void TestNetworkRequest::testPersistentCookieJar()
{
    QString cookieFile = QDir::tempPath() + "/qt_test_cookies_" + QString::number(QCoreApplication::applicationPid()) + ".json";
    QFile::remove(cookieFile);

    NetworkRequestManager::setCookieStoragePath(cookieFile);
    QCOMPARE(NetworkRequestManager::cookieStoragePath(), cookieFile);
    QVERIFY(NetworkRequestManager::cookieJar() != nullptr);

    // First request: set a cookie via the server
    {
        std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
        req->url = s_server->baseUrl() + "/cookies/set?testcookie=hello123";
        req->type = RequestType::Get;

        std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
        QVERIFY(reply != nullptr);

        bool called = false;
        if (reply)
        {
            QObject::connect(reply.get(), &NetworkReply::requestFinished,
                             [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                             {
                                 called = true;
                                 QVERIFY(rsp);
                             });
        }
        QVERIFY(waitForFinished(reply, 30000));
        QVERIFY(called);
    }

    // Verify cookie file was saved
    QVERIFY2(QFile::exists(cookieFile), "Cookie file should exist after request");
    QFile file(cookieFile);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray savedData = file.readAll();
    file.close();
    QVERIFY2(!savedData.isEmpty(), "Cookie file should not be empty");
    qDebug() << "Saved cookies:" << savedData;

    // Second request to /cookies to verify cookie is sent
    {
        std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
        req->url = s_server->baseUrl() + "/cookies";
        req->type = RequestType::Get;

        std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
        QVERIFY(reply != nullptr);

        bool called = false;
        if (reply)
        {
            QObject::connect(reply.get(), &NetworkReply::requestFinished,
                             [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                             {
                                 called = true;
                                 QVERIFY(rsp);
                                 QVERIFY(rsp->success);
                                 QVERIFY(rsp->body.contains("testcookie"));
                             });
        }
        QVERIFY(waitForFinished(reply, 30000));
        QVERIFY(called);
    }

    // Now simulate a new session by creating a new jar from the same file
    NetworkRequestManager::setCookieStoragePath(cookieFile);

    {
        std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
        req->url = s_server->baseUrl() + "/cookies";
        req->type = RequestType::Get;

        std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
        QVERIFY(reply != nullptr);

        bool called = false;
        if (reply)
        {
            QObject::connect(reply.get(), &NetworkReply::requestFinished,
                             [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                             {
                                 called = true;
                                 QVERIFY(rsp);
                                 QVERIFY(rsp->success);
                                 QVERIFY(rsp->body.contains("testcookie"));
                             });
        }
        QVERIFY(waitForFinished(reply, 30000));
        QVERIFY(called);
    }

    QFile::remove(cookieFile);
    NetworkRequestManager::setCookieStoragePath(QString());
}

void TestNetworkRequest::testRequestPriority()
{
    int savedMax = NetworkRequestManager::globalInstance()->maxThreadCount();
    QVERIFY(NetworkRequestManager::globalInstance()->setMaxThreadCount(1));

    std::unique_ptr<RequestContext> highReq = std::make_unique<RequestContext>();
    highReq->url = s_server->baseUrl() + "/get?high=1";
    highReq->type = RequestType::Get;
    highReq->behavior.priority = 10;

    std::shared_ptr<NetworkReply> highReply = NetworkRequestManager::globalInstance()->postRequest(std::move(highReq));
    QVERIFY(highReply != nullptr);

    QCoreApplication::processEvents();
    QThread::msleep(50);

    std::unique_ptr<RequestContext> lowReq = std::make_unique<RequestContext>();
    lowReq->url = s_server->baseUrl() + "/get?low=1";
    lowReq->type = RequestType::Get;
    lowReq->behavior.priority = 0;

    std::shared_ptr<NetworkReply> lowReply = NetworkRequestManager::globalInstance()->postRequest(std::move(lowReq));
    QVERIFY(lowReply != nullptr);

    bool highDone = false, lowDone = false;
    if (highReply)
    {
        QObject::connect(highReply.get(), &NetworkReply::requestFinished,
                         [&highDone](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             highDone = true;
                             QVERIFY(rsp);
                         });
    }
    if (lowReply)
    {
        QObject::connect(lowReply.get(), &NetworkReply::requestFinished,
                         [&lowDone](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             lowDone = true;
                             QVERIFY(rsp);
                         });
    }

    QSignalSpy highSpy(highReply.get(), &NetworkReply::requestFinished);
    QSignalSpy lowSpy(lowReply.get(), &NetworkReply::requestFinished);
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(60000);

    while (timer.isActive() && (!highDone || !lowDone))
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    QVERIFY(highDone);
    QVERIFY(lowDone);

    NetworkRequestManager::globalInstance()->setMaxThreadCount(savedMax);
}

// ─── P0/P1 regression tests ────────────────────────────────────────────────

void TestNetworkRequest::testStopRunningRequest()
{
    // P1-2: Verify cancelling an in-flight request doesn't crash (r.reset() removed).
    // P0-3: Verify cleanup doesn't leak when request is stopped mid-flight.
    // Strategy: start a request to a slow endpoint, cancel it immediately,
    //           and verify the cancelled result is delivered cleanly.

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/delay/5000";
    req->type = RequestType::Get;

    quint64 taskId = 0;
    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    if (reply && reply->task())
    {
        taskId = reply->task()->id;
        QVERIFY(taskId > 0);
    }

    // Set up signal spy BEFORE cancelling (stopRequest emits synchronously)
    QSignalSpy spy(reply.get(), &NetworkReply::requestFinished);

    // Let the request start executing
    QCoreApplication::processEvents();
    QThread::msleep(100);

    // Cancel the in-flight request
    NetworkRequestManager::globalInstance()->stopRequest(taskId);

    // Check result (arrives synchronously via replyResult)
    QVERIFY(!spy.isEmpty());
    QList<QVariant> args = spy.takeFirst();
    auto rsp = args.at(0).value<QSharedPointer<QtNetworkRequest::ResponseResult>>();
    QVERIFY(rsp);
    QVERIFY(!rsp->success);
    QVERIFY(rsp->cancelled);
    QVERIFY(!rsp->body.isEmpty());
}

void TestNetworkRequest::testStopBatchRequest()
{
    // P1-2: Verify stopping a batch of requests mid-execution doesn't crash.
    // The r.reset() was also present in stopBatchRequests().

    BatchRequestPtrTasks tasks;
    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<RequestContext> ctx = std::make_unique<RequestContext>();
        ctx->url = s_server->baseUrl() + "/delay/3000";
        ctx->type = RequestType::Get;
        tasks.push_back(std::move(ctx));
    }

    quint64 batchId = 0;
    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postBatchRequest(std::move(tasks), batchId);
    QVERIFY(reply != nullptr);
    QVERIFY(batchId > 0);

    // Set up spy BEFORE cancelling (stopBatch emits synchronously)
    QSignalSpy spy(reply.get(), &NetworkReply::requestFinished);

    // Let requests start executing
    QCoreApplication::processEvents();
    QThread::msleep(200);

    // Cancel the entire batch
    NetworkRequestManager::globalInstance()->stopBatchRequests(batchId);

    QVERIFY(!spy.isEmpty());
    QList<QVariant> args = spy.takeFirst();
    auto rsp = args.at(0).value<QSharedPointer<QtNetworkRequest::ResponseResult>>();
    QVERIFY(rsp);
    QVERIFY(!rsp->success);
    QVERIFY(rsp->cancelled);
}

void TestNetworkRequest::testStopAllRequests()
{
    // P1-2: Verify stopping all requests in one shot doesn't crash.
    // Covers the r.reset() removal in stopAllRequest().

    // Queue several slow requests
    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
        req->url = s_server->baseUrl() + "/delay/5000";
        req->type = RequestType::Get;
        NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    }

    // Let them start
    QCoreApplication::processEvents();
    QThread::msleep(200);

    // Stop all at once
    NetworkRequestManager::globalInstance()->stopAllRequest();

    // Verify no crash — stopAllRequest() + subsequent operations are safe
    // Also verify new requests can be made afterwards
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?after=stopall";
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->success);
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
}

void TestNetworkRequest::testRapidCancelStress()
{
    // P1-2 + P0-3: Stress test — rapidly create and cancel requests to
    // expose potential use-after-free or race conditions.

    const int iterations = 20;
    int successCount = 0;

    for (int i = 0; i < iterations; ++i)
    {
        std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
        req->url = s_server->baseUrl() + "/delay/2000";
        req->type = RequestType::Get;

        quint64 taskId = 0;
        std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));

        if (reply && reply->task())
        {
            taskId = reply->task()->id;

            // Spy set up BEFORE cancelling
            QSignalSpy spy(reply.get(), &NetworkReply::requestFinished);

            // Let the request enter the thread pool
            QCoreApplication::processEvents();
            QThread::msleep(5);

            // Cancel immediately — result arrives synchronously
            NetworkRequestManager::globalInstance()->stopRequest(taskId);

            if (!spy.isEmpty())
            {
                successCount++;
            }
        }
    }

    // At least 90% of cancel operations should complete normally
    QVERIFY2(successCount >= iterations * 9 / 10,
             qPrintable(QString("Only %1/%2 cancels completed").arg(successCount).arg(iterations)));
}

// ─── Timeout mechanism tests ─────────────────────────────────────────────────

void TestNetworkRequest::testTotalTimeout()
{
    // Layer1: Total request timeout.
    // Verify that a fast request with totalTimeoutMs set still completes normally.

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?fast=1";
    req->type = RequestType::Get;
    req->behavior.totalTimeoutMs = 30000;  // 30s timeout, request will finish fast

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->success);
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testIdleTimeout()
{
    // Layer3: Idle/stall timeout — verify configuration is accepted
    // (The actual timeout behavior requires a real stalled connection,
    //  which is hard to simulate reliably in a unit test environment.)

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?idle=1";
    req->type = RequestType::Get;
    req->behavior.idleTimeoutMs = 5000;  // 5s idle timeout

    // Verify the request completes normally when data flows
    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->success);
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}
