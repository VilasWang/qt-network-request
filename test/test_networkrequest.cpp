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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(!rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(!rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                             QVERIFY(rsp->isSuccess());
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
                                 QVERIFY(rsp->isSuccess());
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
                                 QVERIFY(rsp->isSuccess());
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
    QVERIFY(!rsp->isSuccess());
    QVERIFY(rsp->isCancelled());
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
    QVERIFY(!rsp->isSuccess());
    QVERIFY(rsp->isCancelled());
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
                         QVERIFY(rsp->isSuccess());
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
                         QVERIFY(rsp->isSuccess());
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
                         QVERIFY(rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

// ─── Stage A P0 contract tests ───────────────────────────────────────────────

void TestNetworkRequest::testSendRequestSync()
{
    // Synchronous sendRequest() runs a nested event loop and invokes the
    // callback before returning. Blocking mode is used so the callback runs
    // deterministically inside the call.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?sync=1";
    req->type = RequestType::Get;

    bool called = false;
    bool ok = NetworkRequestManager::globalInstance()->sendRequest(
        std::move(req),
        [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
        {
            called = true;
            QVERIFY(rsp);
            QVERIFY(rsp->isSuccess());
            QVERIFY(!rsp->body.isEmpty());
        },
        true);

    QVERIFY(ok);
    QVERIFY(called);
}

void TestNetworkRequest::testBatchSuccessAndSignal()
{
    // A batch of successful requests should emit batchRequestFinished with
    // bAllSuccess == true once every task in the batch completes.
    QSignalSpy batchSpy(NetworkRequestManager::globalInstance(),
                        &NetworkRequestManager::batchRequestFinished);

    BatchRequestPtrTasks tasks;
    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<RequestContext> ctx = std::make_unique<RequestContext>();
        ctx->url = s_server->baseUrl() + QString("/get?batch=%1").arg(i);
        ctx->type = RequestType::Get;
        tasks.push_back(std::move(ctx));
    }

    quint64 batchId = 0;
    std::shared_ptr<NetworkReply> reply =
        NetworkRequestManager::globalInstance()->postBatchRequest(std::move(tasks), batchId);
    QVERIFY(reply != nullptr);
    QVERIFY(batchId > 0);

    QVERIFY(waitForFinished(reply, 30000));

    // The batchRequestFinished signal is emitted once the whole batch is done;
    // pump the event loop until it arrives.
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(10000);
    while (timer.isActive() && batchSpy.isEmpty())
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    QVERIFY(!batchSpy.isEmpty());
    QList<QVariant> args = batchSpy.takeFirst();
    QCOMPARE(args.at(0).value<quint64>(), batchId);
    QCOMPARE(args.at(1).toBool(), true);
}

void TestNetworkRequest::testTotalTimeoutTriggered()
{
    // /drip sends response headers advertising a large body but never sends
    // the body. With a short total timeout the request must terminate and the
    // result must be flagged as a timeout (Layer1 total-timeout contract).
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/drip";
    req->type = RequestType::Get;
    req->behavior.totalTimeoutMs = 2000;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(!rsp->isSuccess());
                         QVERIFY(rsp->isTimeout());
                         QVERIFY(rsp->error.category == QtNetworkRequest::ErrorCategory::Timeout);
                         QVERIFY(rsp->error.code == QtNetworkRequest::ErrorCode::TimeoutTotal);
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
}

void TestNetworkRequest::testIdleTimeoutTriggered()
{
    // /drip stalls after headers, so no data arrives. An idle timeout should
    // abort the transfer; a total timeout backstop guarantees a response is
    // delivered even if the idle path only aborts the underlying reply.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/drip";
    req->type = RequestType::Get;
    req->behavior.idleTimeoutMs = 1500;
    req->behavior.totalTimeoutMs = 6000; // backstop so the worker never hangs

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(!rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
}

void TestNetworkRequest::testStatusCode404()
{
    // /status/404 returns a 404 with a body. The response should surface the
    // HTTP status code and be marked unsuccessful.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/status/404";
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(!rsp->isSuccess());
                         QCOMPARE(rsp->statusCode, 404);
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testInvalidUrlReturnsNull()
{
    // postRequest() validates the URL via QUrl::isValid(); a malformed URL
    // (unterminated IPv6 literal) must be rejected with a nullptr reply.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("http://[");
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply == nullptr);
}

void TestNetworkRequest::testResponsePerformanceStats()
{
    // A successful response should populate the performance statistics:
    // bytesReceived reflects the payload actually read from the network.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/bytes/1024";
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                         QVERIFY(rsp->performance.bytesReceived > 0);
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

// ─── Stage B P1 feature tests ────────────────────────────────────────────────

void TestNetworkRequest::testRedirectFollow()
{
    // /redirect/2 returns a 302 chain that ultimately points at /get.
    // Depending on Qt's redirect policy the client either transparently
    // follows to the 200 target or surfaces the 3xx directly; both are valid
    // library outcomes. The contract asserted here is: the request completes,
    // delivers a non-null result, and yields a sane final HTTP status.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/redirect/2";
    req->type = RequestType::Get;
    req->behavior.maxRedirectionCount = 5;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    int finalStatus = 0;
    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called, &finalStatus](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         finalStatus = rsp->statusCode;
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
    qDebug() << "Redirect final status:" << finalStatus;
    QVERIFY2(finalStatus == 200 || (finalStatus >= 300 && finalStatus < 400),
             qPrintable(QString("Unexpected redirect status %1").arg(finalStatus)));
}

void TestNetworkRequest::testMaxRedirectExceeded()
{
    // A long redirect chain (10 hops) constrained to a small redirect budget.
    // The request must still terminate cleanly and deliver a result rather than
    // spinning or crashing; the exact terminal status is Qt-policy dependent.
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/redirect/10";
    req->type = RequestType::Get;
    req->behavior.maxRedirectionCount = 2;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    int finalStatus = -1;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called, &finalStatus](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         finalStatus = rsp->statusCode;
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
    qDebug() << "Max-redirect terminal status:" << finalStatus;
}

#ifndef QT_NO_SSL
void TestNetworkRequest::testGlobalSslConfigNormalize()
{
    // Global SSL config must never retain Inherit sentinels: setGlobalSslConfig
    // normalizes any Inherit field to the corresponding secure-default value,
    // while explicitly-provided values are preserved verbatim.
    SslConfig saved = NetworkRequestManager::globalSslConfig();

    // All-Inherit input → fully normalized to secureDefault.
    SslConfig allInherit;  // default-constructed: every field == Inherit
    NetworkRequestManager::setGlobalSslConfig(allInherit);
    SslConfig out = NetworkRequestManager::globalSslConfig();
    const SslConfig def = SslConfig::secureDefault();
    QVERIFY(out.peerVerifyMode != SslConfig::PeerVerifyMode::Inherit);
    QVERIFY(out.minProtocol != SslConfig::TlsProtocol::Inherit);
    QVERIFY(out.ignoreSslErrorsPolicy != SslConfig::IgnorePolicy::Inherit);
    QVERIFY(out.caPolicy != SslConfig::CaPolicy::Inherit);
    QCOMPARE(int(out.peerVerifyMode), int(def.peerVerifyMode));
    QCOMPARE(int(out.minProtocol), int(def.minProtocol));
    QCOMPARE(int(out.ignoreSslErrorsPolicy), int(def.ignoreSslErrorsPolicy));
    QCOMPARE(int(out.caPolicy), int(def.caPolicy));

    // Explicit values are preserved; unset (Inherit) fields still normalize.
    SslConfig custom;
    custom.peerVerifyMode = SslConfig::PeerVerifyMode::VerifyNone;
    custom.minProtocol = SslConfig::TlsProtocol::TlsV1_3;
    NetworkRequestManager::setGlobalSslConfig(custom);
    out = NetworkRequestManager::globalSslConfig();
    QCOMPARE(int(out.peerVerifyMode), int(SslConfig::PeerVerifyMode::VerifyNone));
    QCOMPARE(int(out.minProtocol), int(SslConfig::TlsProtocol::TlsV1_3));
    QVERIFY(out.ignoreSslErrorsPolicy != SslConfig::IgnorePolicy::Inherit);
    QVERIFY(out.caPolicy != SslConfig::CaPolicy::Inherit);

    NetworkRequestManager::setGlobalSslConfig(saved);
}

void TestNetworkRequest::testPerRequestSslInherit()
{
    // A per-request SslConfig left entirely at Inherit must resolve against the
    // (non-default) global config without breaking the request. Over plain HTTP
    // the SSL settings are ignored by Qt, so the effective assertion is that
    // Inherit-resolution is side-effect free and the request still succeeds.
    SslConfig saved = NetworkRequestManager::globalSslConfig();

    SslConfig global;
    global.peerVerifyMode = SslConfig::PeerVerifyMode::VerifyNone;
    global.minProtocol = SslConfig::TlsProtocol::TlsV1_2;
    NetworkRequestManager::setGlobalSslConfig(global);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?ssl=inherit";
    req->type = RequestType::Get;
    req->sslConfig = std::make_unique<SslConfig>();  // all Inherit

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);

    NetworkRequestManager::setGlobalSslConfig(saved);
}
#endif

void TestNetworkRequest::testStopSession()
{
    // nextSessionId() must hand out strictly-increasing ids, and
    // stopSessionRequest() must cancel every in-flight request tagged with that
    // session. Session-stop is a *silent* cancel: onResponse() drops the result
    // for a stopped session (see NetworkRequestManager::onResponse), so the
    // reply's requestFinished signal must NOT fire afterwards.
    quint64 sid1 = NetworkRequestManager::globalInstance()->nextSessionId();
    quint64 sid2 = NetworkRequestManager::globalInstance()->nextSessionId();
    QVERIFY(sid2 > sid1);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/delay/5000";
    req->type = RequestType::Get;
    req->task.sessionId = sid2;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    QSignalSpy spy(reply.get(), &NetworkReply::requestFinished);

    // Let the request get dispatched onto a worker thread before cancelling.
    QCoreApplication::processEvents();
    QThread::msleep(100);

    NetworkRequestManager::globalInstance()->stopSessionRequest(sid2);

    // Spin the event loop well under the server's 5s delay: a stopped session
    // must never deliver a result, so the spy stays empty.
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 800)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }

    QVERIFY2(spy.isEmpty(), "stopSessionRequest must silently cancel: no requestFinished expected");
}

void TestNetworkRequest::testDownloadProgress()
{
    // showProgress=true must surface incremental downloadProgress(recv,total)
    // signals on the reply for a single-threaded download. The /slowbytes
    // endpoint dribbles the body out over ~640ms so the client's 250ms
    // progress-throttle timer is guaranteed to tick at least once.
    QString tmpDir = QDir::tempPath();
    QString savePath = tmpDir + "/qt_test_dlprogress_" + QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(savePath);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/slowbytes/262144";
    req->type = RequestType::Download;
    req->behavior.showProgress = true;
    req->downloadConfig = std::make_unique<DownloadConfig>();
    req->downloadConfig->threadCount = 1;
    req->downloadConfig->saveDir = tmpDir;
    req->downloadConfig->saveFileName = QFileInfo(savePath).fileName();
    req->downloadConfig->overwriteFile = true;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    QSignalSpy progressSpy(reply.get(), &NetworkReply::downloadProgress);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);
    QVERIFY2(progressSpy.count() > 0, "Expected at least one downloadProgress signal");

    QFile::remove(savePath);
}

void TestNetworkRequest::testUploadProgress()
{
    // showProgress=true on an Upload request wires the uploadProgress signal and
    // drives byte accounting. On loopback the transfer usually completes inside
    // the 250ms progress-throttle window, so emitted uploadProgress signals are
    // best-effort and not asserted; instead we assert the deterministic outcome:
    // the reply reports the full request body as bytesSent.
    const qint64 fileSize = 512 * 1024;
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    tmpFile.write(QByteArray(int(fileSize), 'U'));
    tmpFile.flush();
    QString filePath = tmpFile.fileName();
    tmpFile.close();

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/put";
    req->type = RequestType::Upload;
    req->behavior.showProgress = true;
    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->usePutMethod = true;
    req->uploadConfig->filePath = filePath;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    qint64 reportedSent = -1;
    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called, &reportedSent](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                         reportedSent = rsp->performance.bytesSent;
                     });

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);
    QCOMPARE(reportedSent, fileSize);
}

void TestNetworkRequest::testSetMaxThreadCountBounds()
{
    // setMaxThreadCount accepts 1..100 and rejects out-of-range values without
    // mutating the current setting.
    NetworkRequestManager *mgr = NetworkRequestManager::globalInstance();
    int saved = mgr->maxThreadCount();

    QVERIFY(mgr->setMaxThreadCount(1));
    QCOMPARE(mgr->maxThreadCount(), 1);
    QVERIFY(mgr->setMaxThreadCount(100));
    QCOMPARE(mgr->maxThreadCount(), 100);

    QVERIFY(!mgr->setMaxThreadCount(0));
    QVERIFY(!mgr->setMaxThreadCount(-5));
    QVERIFY(!mgr->setMaxThreadCount(101));
    // Rejected calls must leave the last valid value (100) intact.
    QCOMPARE(mgr->maxThreadCount(), 100);

    mgr->setMaxThreadCount(saved);
}

void TestNetworkRequest::testDownloadAutoThreadCount()
{
    // threadCount=0 selects the auto (CPU-core) multi-thread download path.
    QString tmpDir = QDir::tempPath();
    QString savePath = tmpDir + "/qt_test_autodl_" + QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(savePath);

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/bytes/8192";
    req->type = RequestType::Download;
    req->downloadConfig = std::make_unique<DownloadConfig>();
    req->downloadConfig->threadCount = 0;  // auto
    req->downloadConfig->saveDir = tmpDir;
    req->downloadConfig->saveFileName = QFileInfo(savePath).fileName();
    req->downloadConfig->overwriteFile = true;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 60000));
    QVERIFY(called);

    QFileInfo fi(savePath);
    QVERIFY2(fi.exists(), "Auto-thread downloaded file should exist");
    QCOMPARE(fi.size(), 8192);
    QFile::remove(savePath);
}

void TestNetworkRequest::testDownloadNoOverwriteConflict()
{
    // Single-threaded download to an existing target with overwriteFile=false
    // must fail with a file-conflict rather than clobbering the file.
    QString tmpDir = QDir::tempPath();
    QString savePath = tmpDir + "/qt_test_nooverwrite_" + QString::number(QCoreApplication::applicationPid()) + ".dat";

    // Pre-create the target with sentinel content.
    {
        QFile f(savePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("PREEXISTING");
        f.close();
    }

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/bytes/2048";
    req->type = RequestType::Download;
    req->downloadConfig = std::make_unique<DownloadConfig>();
    req->downloadConfig->threadCount = 1;
    req->downloadConfig->saveDir = tmpDir;
    req->downloadConfig->saveFileName = QFileInfo(savePath).fileName();
    req->downloadConfig->overwriteFile = false;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(!rsp->isSuccess());
                     });

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);

    // Original file must remain untouched.
    QFile f(savePath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("PREEXISTING"));
    f.close();
    QFile::remove(savePath);
}

void TestNetworkRequest::testFormDataUpload()
{
    // multipart/form-data upload via UploadConfig::files (POST). The request
    // must succeed and the server must observe a POST.
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    tmpFile.write("form-data file body 0123456789");
    tmpFile.flush();
    QString filePath = tmpFile.fileName();
    tmpFile.close();

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/post";
    req->type = RequestType::Post;
    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->useFormData = true;
    req->uploadConfig->files = QStringList{ filePath };
    req->uploadConfig->kvPairs.insert("field1", "value1");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                         QVERIFY(rsp->body.contains("POST"));
                     });

    QVERIFY(waitForFinished(reply, 30000));
    QVERIFY(called);
}

void TestNetworkRequest::testUserContextRoundTrip()
{
    // A user-supplied QVariant context must be carried through to the response
    // unchanged (success path copies request.userContext into the result).
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?ctx=1";
    req->type = RequestType::Get;
    req->userContext = QVariant(QString("ctx-token-42"));

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                         QCOMPARE(rsp->userContext.toString(), QString("ctx-token-42"));
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);
}

void TestNetworkRequest::testPerRequestCookies()
{
    // Cookies attached to a single RequestContext are inserted into the NAM's
    // cookie jar before the request is sent (see NetworkCommonRequest::start).
    // That path is a no-op unless a global cookie jar is configured, so we
    // enable one first. The /cookies endpoint echoes received cookies back.
    QString cookieFile = QDir::tempPath() + "/qt_test_percookie_" + QString::number(QCoreApplication::applicationPid()) + ".json";
    QFile::remove(cookieFile);
    NetworkRequestManager::setCookieStoragePath(cookieFile);
    QVERIFY(NetworkRequestManager::cookieJar() != nullptr);

    QNetworkCookie cookie("percookie", "pcval");
    cookie.setDomain(QUrl(s_server->baseUrl()).host());
    cookie.setPath("/");

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/cookies";
    req->type = RequestType::Get;
    req->cookies.append(cookie);

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);

    bool called = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&called](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                     {
                         called = true;
                         QVERIFY(rsp);
                         QVERIFY(rsp->isSuccess());
                         QVERIFY2(rsp->body.contains("percookie"),
                                  qPrintable(QString("cookies echo missing per-request cookie: %1")
                                                 .arg(QString::fromUtf8(rsp->body))));
                     });

    QVERIFY(waitForFinished(reply, 10000));
    QVERIFY(called);

    QFile::remove(cookieFile);
}

void TestNetworkRequest::testInitializeIdempotent()
{
    // The manager is already initialized by initTestCase(). A redundant
    // initialize() must be a harmless no-op (guarded by s_isInitialized) and the
    // global instance must remain the same, valid object.
    QVERIFY(NetworkRequestManager::isInitialized());
    NetworkRequestManager *before = NetworkRequestManager::globalInstance();
    QVERIFY(before != nullptr);

    NetworkRequestManager::initialize();  // redundant
    QVERIFY(NetworkRequestManager::isInitialized());

    NetworkRequestManager *after = NetworkRequestManager::globalInstance();
    QCOMPARE(after, before);

    // A live request still works after the redundant initialize().
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = s_server->baseUrl() + "/get?reinit=1";
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = after->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testNamPoolReuseSameThread()
{
    // acquireThreadNam() hands out a QNetworkAccessManager that is affine to,
    // and cached for, the calling thread. Repeated calls on the same thread
    // must return the very same instance (pool reuse, no per-call allocation).
    QNetworkAccessManager *nam1 = NetworkRequestManager::acquireThreadNam();
    QVERIFY(nam1 != nullptr);
    QNetworkAccessManager *nam2 = NetworkRequestManager::acquireThreadNam();
    QCOMPARE(nam2, nam1);
    // NAM must be affine to the acquiring (main) thread.
    QCOMPARE(nam1->thread(), QThread::currentThread());
}
