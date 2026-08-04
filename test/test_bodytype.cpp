#include "test_bodytype.h"
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include "httptestserver.h"

using namespace QtNetworkRequest;

BodyTypeTests::BodyTypeTests()
    : QObject(nullptr)
{
}

void BodyTypeTests::initTestCase()
{
    m_manager = NetworkRequestManager::globalInstance();
    QVERIFY(m_manager != nullptr);
}

void BodyTypeTests::cleanupTestCase()
{
}

void BodyTypeTests::waitForResponse(int timeoutMs)
{
    m_responseReceived = false;
    m_lastResult.clear();
    QElapsedTimer timer;
    timer.start();
    while (!m_responseReceived && timer.elapsed() < timeoutMs)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

// ============================================================================
// No-network builder validation tests
// ============================================================================

void BodyTypeTests::testBodyJsonBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .bodyJson("{\"key\":\"value\"}")
        .build();

    QVERIFY(ctx->bodyType == BodyType::Json);
    QCOMPARE(ctx->body, QString("{\"key\":\"value\"}"));
}

void BodyTypeTests::testBodyXmlBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .bodyXml("<root><item>value</item></root>")
        .build();

    QVERIFY(ctx->bodyType == BodyType::Xml);
    QCOMPARE(ctx->body, QString("<root><item>value</item></root>"));
}

void BodyTypeTests::testBodyFormUrlEncodedBuilder()
{
    QMap<QString, QString> params;
    params["name"] = "John";
    params["age"] = "30";

    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .bodyFormUrlEncoded(params)
        .build();

    QVERIFY(ctx->bodyType == BodyType::FormUrlEncoded);
    // Should be URL-encoded: "name=John&age=30" (order may vary)
    QVERIFY(ctx->body.contains("name=John") || ctx->body.contains("name%3DJohn"));
    QVERIFY(ctx->body.contains("age=30") || ctx->body.contains("age%3D30"));
}

void BodyTypeTests::testBodyRawBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .bodyRaw("some plain text")
        .build();

    QVERIFY(ctx->bodyType == BodyType::Raw);
    QCOMPARE(ctx->body, QString("some plain text"));
}

void BodyTypeTests::testBodyBinaryBuilder()
{
    // 11 bytes: "binary" + 0x00 + "data". Hex escape "\x00data" would greedily
    // consume 'd','a' -> 0x00DA, losing the embedded null we intend to test.
    char raw[] = {'b', 'i', 'n', 'a', 'r', 'y', '\0', 'd', 'a', 't', 'a'};
    QByteArray data(raw, 11);

    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .bodyBinary(data)
        .build();

    QVERIFY(ctx->bodyType == BodyType::Binary);
    QCOMPARE(ctx->binaryBody, data);
    QVERIFY(ctx->body.isEmpty());
}

void BodyTypeTests::testBodyTypeDefault()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Post)
        .body("some body")
        .build();

    // Default bodyType is None (backward compatible)
    QVERIFY(ctx->bodyType == BodyType::None);
    QCOMPARE(ctx->body, QString("some body"));
}

// ============================================================================
// Network integration tests
// ============================================================================

void BodyTypeTests::testBodyJsonContentType()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/post";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Post)
        .bodyJson("{\"test\":true}")
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());

    // Verify the server received Content-Type: application/json
    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["content-type"].toString(), QString("application/json"));
}

void BodyTypeTests::testBodyXmlContentType()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/post";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Post)
        .bodyXml("<root/>")
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());

    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["content-type"].toString(), QString("application/xml"));
}

void BodyTypeTests::testBodyFormUrlEncodedContentType()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/post";

    QMap<QString, QString> params;
    params["name"] = "test";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Post)
        .bodyFormUrlEncoded(params)
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());

    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["content-type"].toString(), QString("application/x-www-form-urlencoded"));
}

void BodyTypeTests::testBodyBinaryRequest()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/post";

    QByteArray binaryData("hello binary world", 18);

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Post)
        .bodyBinary(binaryData)
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());

    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["content-type"].toString(), QString("application/octet-stream"));
}

void BodyTypeTests::testBodyCTPriority()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/post";

    // User explicitly sets Content-Type; bodyType auto-detection should NOT override it
    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Post)
        .bodyJson("{\"test\":true}")
        .header("Content-Type", "application/vnd.api+json")
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());

    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["content-type"].toString(), QString("application/vnd.api+json"));
}

// Tests run via main_requestbuilder.cpp entry point
#include "test_bodytype.moc"
