#include "test_networkrequest.h"
#include <QTimer>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QThread>
#include <QHttpPart>
#include <QObject>
#include <QSharedPointer>

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