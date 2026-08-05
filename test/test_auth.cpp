#include "test_auth.h"
#include <QtTest/QtTest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include "httptestserver.h"

using namespace QtNetworkRequest;

AuthTests::AuthTests()
    : QObject(nullptr)
{
}

void AuthTests::initTestCase()
{
    NetworkRequestManager::initialize();
    m_manager = NetworkRequestManager::globalInstance();
    QVERIFY(m_manager != nullptr);
}

void AuthTests::cleanupTestCase()
{
    NetworkRequestManager::unInitialize();
}

void AuthTests::waitForResponse(int timeoutMs)
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

void AuthTests::testAuthBasicBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Get)
        .authBasic("alice", "secret")
        .build();

    QVERIFY(ctx->authConfig.type == AuthType::Basic);
    QCOMPARE(ctx->authConfig.username, QString("alice"));
    QCOMPARE(ctx->authConfig.password, QString("secret"));
}

void AuthTests::testAuthBearerBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Get)
        .authBearer("tok-abc123")
        .build();

    QVERIFY(ctx->authConfig.type == AuthType::Bearer);
    QCOMPARE(ctx->authConfig.token, QString("tok-abc123"));
}

void AuthTests::testAuthApiKeyHeaderBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Get)
        .authApiKey("X-API-Key", "my-secret-key")
        .build();

    QVERIFY(ctx->authConfig.type == AuthType::ApiKey);
    QCOMPARE(ctx->authConfig.apiKey, QString("X-API-Key"));
    QCOMPARE(ctx->authConfig.apiValue, QString("my-secret-key"));
    QVERIFY(ctx->authConfig.apiKeyPlacement == ApiKeyPlacement::Header);
}

void AuthTests::testAuthApiKeyQueryBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com")
        .type(RequestType::Get)
        .authApiKey("apikey", "val123", ApiKeyPlacement::QueryParam)
        .build();

    QVERIFY(ctx->authConfig.type == AuthType::ApiKey);
    QVERIFY(ctx->authConfig.apiKeyPlacement == ApiKeyPlacement::QueryParam);
}

void AuthTests::testAuthConfigIsValid()
{
    // Valid cases
    QVERIFY(AuthConfig::basic("user", "pass").isValid());
    QVERIFY(AuthConfig::bearer("token").isValid());
    QVERIFY(AuthConfig::apiKeyAuth("key", "val").isValid());

    // None is always valid
    AuthConfig none;
    QVERIFY(none.isValid());

    // Invalid cases
    QVERIFY(!AuthConfig::basic("", "pass").isValid());   // empty username
    QVERIFY(!AuthConfig::bearer("").isValid());            // empty token
    QVERIFY(!AuthConfig::apiKeyAuth("", "val").isValid());     // empty key
    QVERIFY(!AuthConfig::apiKeyAuth("key", "").isValid());     // empty value
}

void AuthTests::testAuthBasicHeaderValue()
{
    AuthConfig cfg = AuthConfig::basic("user", "pass");
    QByteArray headerVal = cfg.authorizationHeaderValue();

    QVERIFY(headerVal.startsWith("Basic "));
    // "user:pass" base64 encoded = "dXNlcjpwYXNz"
    QCOMPARE(headerVal, QByteArray("Basic dXNlcjpwYXNz"));
}

void AuthTests::testAuthBearerHeaderValue()
{
    AuthConfig cfg = AuthConfig::bearer("my-token");
    QByteArray headerVal = cfg.authorizationHeaderValue();

    QCOMPARE(headerVal, QByteArray("Bearer my-token"));
}

void AuthTests::testAuthNoneNoHeader()
{
    AuthConfig cfg;
    QVERIFY(cfg.type == AuthType::None);
    QVERIFY(cfg.authorizationHeaderValue().isEmpty());
}

// ============================================================================
// Network integration tests using local HTTP test server
// ============================================================================

void AuthTests::testAuthBasicRequest()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/get";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Get)
        .authBasic("admin", "test123")
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse();

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult != nullptr);
    QVERIFY(m_lastResult->isSuccess());

    // Verify the server echoed back the Authorization header
    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QString authHeader = headers["authorization"].toString();
    QVERIFY(authHeader.startsWith("Basic "));
}

void AuthTests::testAuthBearerRequest()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/get";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Get)
        .authBearer("tok-test-456")
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
    QCOMPARE(headers["authorization"].toString(), QString("Bearer tok-test-456"));
}

void AuthTests::testAuthApiKeyHeaderRequest()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/get";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Get)
        .authApiKey("X-Custom-Key", "custom-value-789")
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
    QCOMPARE(headers["x-custom-key"].toString(), QString("custom-value-789"));
}

void AuthTests::testAuthApiKeyQueryRequest()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/get";

    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Get)
        .authApiKey("token", "query-token-xyz", ApiKeyPlacement::QueryParam)
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

    // The test server echoes the URL; verify query param is present
    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject args = doc.object()["args"].toObject();
    QCOMPARE(args["token"].toString(), QString("query-token-xyz"));
}

void AuthTests::testAuthHeaderPriority()
{
    HttpTestServer server;
    QVERIFY(server.start());

    QString url = server.baseUrl() + "/get";

    // User explicitly sets Authorization header; authBasic should NOT overwrite it
    auto ctx = RequestContextBuilder()
        .url(url)
        .type(RequestType::Get)
        .header("Authorization", "CustomAuthValue")
        .authBasic("user", "pass")
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
    QCOMPARE(headers["authorization"].toString(), QString("CustomAuthValue"));
}

void AuthTests::testAuthInvalidCredentials()
{
    // Basic auth with empty username should produce Configuration error
    auto ctx = RequestContextBuilder()
        .url("https://example.com/api")
        .type(RequestType::Get)
        .authBasic("", "pass")
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse(3000);

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult != nullptr);
    QVERIFY(!m_lastResult->isSuccess());
    QVERIFY(m_lastResult->error.category == ErrorCategory::Configuration);
    QVERIFY(m_lastResult->error.code == ErrorCode::AuthInvalid);
}

// ============================================================================
// OAuth2 integration tests (M2)
// ============================================================================

void AuthTests::testOAuth2ClientCredentials()
{
    HttpTestServer server;
    QVERIFY(server.start());

    AuthConfig::OAuth2Config oa;
    oa.grant        = OAuth2GrantType::ClientCredentials;
    oa.clientId     = "test-client";
    oa.clientSecret = "test-secret";
    oa.tokenUrl     = server.baseUrl() + "/oauth/token";

    auto ctx = RequestContextBuilder()
        .url(server.baseUrl() + "/get")
        .type(RequestType::Get)
        .authOAuth2(oa)
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    QVERIFY(reply != nullptr);

    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });

    waitForResponse(10000);

    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());
    QCOMPARE(m_lastResult->statusCode, 200);

    // Verify the Bearer token was sent
    QJsonDocument doc = QJsonDocument::fromJson(m_lastResult->body);
    QJsonObject headers = doc.object()["headers"].toObject();
    QCOMPARE(headers["authorization"].toString(), QString("Bearer test-access-client_credentials"));
}

void AuthTests::testOAuth2ClientCredentialsCache()
{
    HttpTestServer server;
    QVERIFY(server.start());

    AuthConfig::OAuth2Config oa;
    oa.grant        = OAuth2GrantType::ClientCredentials;
    oa.clientId     = "cached-client";
    oa.clientSecret = "cached-secret";
    oa.tokenUrl     = server.baseUrl() + "/oauth/token";

    // First request — must fetch a new token
    auto ctx1 = RequestContextBuilder()
        .url(server.baseUrl() + "/get")
        .type(RequestType::Get)
        .authOAuth2(oa)
        .build();

    auto reply1 = m_manager->postRequest(std::move(ctx1));
    connect(reply1.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });
    waitForResponse(10000);
    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());
    QCOMPARE(m_lastResult->statusCode, 200);

    // Second request with same config — cache should hit
    m_responseReceived = false;
    auto ctx2 = RequestContextBuilder()
        .url(server.baseUrl() + "/get")
        .type(RequestType::Get)
        .authOAuth2(oa)
        .build();

    auto reply2 = m_manager->postRequest(std::move(ctx2));
    connect(reply2.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });
    waitForResponse(5000);
    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());
}

void AuthTests::testOAuth2PasswordGrant()
{
    HttpTestServer server;
    QVERIFY(server.start());

    AuthConfig::OAuth2Config oa;
    oa.grant        = OAuth2GrantType::Password;
    oa.clientId     = "password-client";
    oa.clientSecret = "password-secret";
    oa.username     = "testuser";
    oa.password     = "testpass";
    oa.tokenUrl     = server.baseUrl() + "/oauth/token";

    auto ctx = RequestContextBuilder()
        .url(server.baseUrl() + "/get")
        .type(RequestType::Get)
        .authOAuth2(oa)
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });
    waitForResponse(10000);
    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());
}

void AuthTests::testOAuth2RefreshToken()
{
    HttpTestServer server;
    QVERIFY(server.start());

    AuthConfig::OAuth2Config oa;
    oa.grant        = OAuth2GrantType::RefreshToken;
    oa.clientId     = "refresh-client";
    oa.clientSecret = "refresh-secret";
    oa.refreshToken = "test-refresh-token";
    oa.tokenUrl     = server.baseUrl() + "/oauth/token";

    auto ctx = RequestContextBuilder()
        .url(server.baseUrl() + "/get")
        .type(RequestType::Get)
        .authOAuth2(oa)
        .build();

    auto reply = m_manager->postRequest(std::move(ctx));
    connect(reply.get(), &NetworkReply::requestFinished, this, [this](QSharedPointer<ResponseResult> rsp) {
        m_lastResult = rsp;
        m_responseReceived = true;
    });
    waitForResponse(10000);
    QVERIFY(m_responseReceived);
    QVERIFY(m_lastResult->isSuccess());
}

void AuthTests::testOAuth2TokenExpired()
{
    // Test that an invalid (no tokenUrl) OAuth2 config is rejected by isValid()
    AuthConfig::OAuth2Config oa;
    oa.grant    = OAuth2GrantType::ClientCredentials;
    oa.clientId = "test";
    // tokenUrl is empty — should be rejected
    AuthConfig ac = AuthConfig::oauth2(oa);
    QVERIFY(!ac.isValid());
    QCOMPARE(ac.type, AuthType::OAuth2);
}

// Tests run via main_auth.cpp entry point
#include "test_auth.moc"
