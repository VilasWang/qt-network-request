#include "test_networkrequest.h"
#include <QTimer>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QThread>
#include <QHttpPart>
#include <QObject>
#include <QSharedPointer>
#include <QElapsedTimer>

using namespace QtNetworkRequest;

void TestNetworkRequest::initTestCase()
{
    // Register ResponseResult type to Qt meta-object system
    qRegisterMetaType<QSharedPointer<QtNetworkRequest::ResponseResult>>("QSharedPointer<QtNetworkRequest::ResponseResult>");

    // Initialize network request manager
    NetworkRequestManager::initialize();
    QVERIFY(NetworkRequestManager::isInitialized());
}

void TestNetworkRequest::cleanupTestCase()
{
    // Clean up network request manager
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
    // Test GET request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/get?test1=1&test2=2");
    req->type = RequestType::Get;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }
    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testPostRequest()
{
    // Test POST request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/post");
    req->type = RequestType::Post;
    req->body = QString("{\"test\": \"data\"}");

    // Set Content-Type
    req->headers.insert("Content-Type", "application/json");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testPostFormDataRequest()
{
    // Test POST form data request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/post");
    req->type = RequestType::Post;

    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->useFormData = true;
    req->uploadConfig->kvPairs.insert("key", "value");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testPutRequest()
{
    // Test PUT request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/put");
    req->type = RequestType::Put;

    // Set Content-Type
    req->headers.insert("Content-Type", "application/json");

    req->uploadConfig = std::make_unique<UploadConfig>();
    req->uploadConfig->usePutMethod = true;
    req->uploadConfig->useStream = true;
    req->uploadConfig->data = QString("{\"test\": \"put_data\"}").toUtf8();

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testDeleteRequest()
{
    // Test DELETE request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/delete");
    req->type = RequestType::Delete;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testHeadRequest()
{
    // Test HEAD request
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/get");
    req->type = RequestType::Head;

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->headers.isEmpty());
                         });
    }
    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testRequestHeaders()
{
    // Test request header handling
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/headers");
    req->type = RequestType::Get;

    // Add custom request headers
    req->headers.insert("X-Custom-Header", "CustomValue");
    req->headers.insert("Accept", "application/json");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
}

void TestNetworkRequest::testContentType()
{
    // Test Content-Type handling
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/post");
    req->type = RequestType::Post;
    req->body = QString("key1=value1&key2=value2");

    // Explicitly set Content-Type
    req->headers.insert("Content-Type", "application/x-www-form-urlencoded");

    std::shared_ptr<NetworkReply> reply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    QVERIFY(reply != nullptr);
    if (reply)
    {
        QObject::connect(reply.get(), &NetworkReply::requestFinished,
                         [this](QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
                         {
                             // Check if request is successful
                             QVERIFY(rsp);
                             QVERIFY(rsp->success);
                             QVERIFY(!rsp->body.isEmpty());
                         });
    }

    // Wait for request to complete
    QVERIFY(waitForFinished(reply, 10000));
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

    // Reset
    ProxyConfig empty;
    NetworkRequestManager::setGlobalProxy(empty);
}

void TestNetworkRequest::testRequestProxyConfig()
{
    // Request with per-request proxy will fail because proxy is unreachable,
    // but the fact that it fails proves the proxy was applied
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("https://httpbin.org/get");
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
                             // Should fail because 127.0.0.1:1 is not a valid proxy
                             QVERIFY(!rsp->success);
                         });
    }

    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);
}

void TestNetworkRequest::testRetryOnFailure()
{
    // Request to unreachable port with retry enabled
    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = QString("http://127.0.0.1:1/");
    req->type = RequestType::Get;
    req->behavior.retryOnFailed = true;
    req->behavior.maxRetryCount = 2;
    req->behavior.retryDelayMs = 100;
    req->behavior.transferTimeout = 2000;

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
                             QVERIFY(!rsp->success);
                             QVERIFY(!rsp->errorMessage.isEmpty());
                         });
    }

    // Test should complete within reasonable time (retry delay = 100 + 200 = 300ms + request time)
    QVERIFY(waitForFinished(reply, 15000));
    QVERIFY(called);

    qint64 elapsed = timer.elapsed();
    QVERIFY(elapsed >= 100); // At least one retry delay
    QVERIFY(elapsed < 30000); // Sanity check

    // Test without retry for comparison
    std::unique_ptr<RequestContext> reqNoRetry = std::make_unique<RequestContext>();
    reqNoRetry->url = QString("http://127.0.0.1:1/");
    reqNoRetry->type = RequestType::Get;
    reqNoRetry->behavior.retryOnFailed = false;
    reqNoRetry->behavior.transferTimeout = 2000;

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

    QVERIFY(waitForFinished(replyNoRetry, 15000));
    QVERIFY(calledNoRetry);

    qint64 elapsedNoRetry = timerNoRetry.elapsed();

    // With retries, the request should take noticeably longer
    // (no-retry should complete faster than the retry case)
    qDebug() << "Retry elapsed:" << elapsed << "ms, No-retry elapsed:" << elapsedNoRetry << "ms";
}