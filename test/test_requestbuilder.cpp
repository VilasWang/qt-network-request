#include "test_requestbuilder.h"
#include "requestcontext.h"
#include "proxyconfig.h"
#ifndef QT_NO_SSL
#include "sslconfig.h"
#endif

using namespace QtNetworkRequest;

void TestRequestBuilder::testBasicFields()
{
    auto ctx = RequestContextBuilder()
                   .url("http://example.com/api")
                   .type(RequestType::Post)
                   .body("hello=world")
                   .build();

    QVERIFY(ctx != nullptr);
    QCOMPARE(ctx->url, QString("http://example.com/api"));
    QVERIFY(ctx->type == RequestType::Post);
    QCOMPARE(ctx->body, QString("hello=world"));
}

void TestRequestBuilder::testHeaders()
{
    // Single header() calls accumulate.
    auto ctx = RequestContextBuilder()
                   .url("http://example.com")
                   .type(RequestType::Get)
                   .header("X-A", "1")
                   .header("X-B", "2")
                   .build();

    QCOMPARE(ctx->headers.size(), 2);
    QCOMPARE(ctx->headers.value("X-A"), QByteArray("1"));
    QCOMPARE(ctx->headers.value("X-B"), QByteArray("2"));

    // headers() replaces the whole map.
    QMap<QByteArray, QByteArray> all;
    all.insert("Content-Type", "application/json");
    auto ctx2 = RequestContextBuilder()
                    .url("http://example.com")
                    .headers(all)
                    .build();
    QCOMPARE(ctx2->headers.size(), 1);
    QCOMPARE(ctx2->headers.value("Content-Type"), QByteArray("application/json"));
}

void TestRequestBuilder::testBehaviorFields()
{
    auto ctx = RequestContextBuilder()
                   .url("http://example.com")
                   .type(RequestType::Get)
                   .timeout(12000)
                   .transferTimeout(4000)
                   .idleTimeout(2000)
                   .priority(7)
                   .showProgress(true)
                   .build();

    QCOMPARE(ctx->behavior.totalTimeoutMs, 12000);
    QCOMPARE(ctx->behavior.transferTimeout, 4000);
    QCOMPARE(ctx->behavior.idleTimeoutMs, 2000);
    QCOMPARE(ctx->behavior.priority, 7);
    QCOMPARE(ctx->behavior.showProgress, true);
}

void TestRequestBuilder::testRetryAndRedirects()
{
    auto ctx = RequestContextBuilder()
                   .url("http://example.com")
                   .retry(5, 250)
                   .maxRedirects(9)
                   .build();

    QCOMPARE(ctx->behavior.retryOnFailed, true);
    QCOMPARE(ctx->behavior.maxRetryCount, static_cast<quint16>(5));
    QCOMPARE(ctx->behavior.retryDelayMs, 250);
    QCOMPARE(ctx->behavior.maxRedirectionCount, static_cast<quint16>(9));
}

void TestRequestBuilder::testTaskAndSessionIds()
{
    auto ctx = RequestContextBuilder()
                   .url("http://example.com")
                   .taskId(11)
                   .batchId(22)
                   .sessionId(33)
                   .build();

    QCOMPARE(ctx->task.id, static_cast<quint64>(11));
    QCOMPARE(ctx->task.batchId, static_cast<quint64>(22));
    QCOMPARE(ctx->task.sessionId, static_cast<quint64>(33));
}

void TestRequestBuilder::testConfigsAndUserContext()
{
    auto proxy = std::make_unique<ProxyConfig>();
    proxy->enabled = true;
    proxy->host = "127.0.0.1";
    proxy->port = 8888;

    auto dl = std::make_unique<DownloadConfig>();
    dl->saveDir = "/tmp";
    dl->saveFileName = "f.bin";
    dl->threadCount = 4;

    auto ul = std::make_unique<UploadConfig>();
    ul->usePutMethod = true;

    RequestContextBuilder builder;
    builder.url("http://example.com")
        .type(RequestType::Download)
        .proxyConfig(std::move(proxy))
        .downloadConfig(std::move(dl))
        .uploadConfig(std::move(ul))
        .userContext(QVariant(QString("ctx-token")));
#ifndef QT_NO_SSL
    SslConfig ssl;
    ssl.peerVerifyMode = SslConfig::PeerVerifyMode::VerifyNone;
    builder.sslConfig(ssl);
#endif
    auto ctx = builder.build();

    QVERIFY(ctx->proxyConfig != nullptr);
    QVERIFY(ctx->proxyConfig->enabled);
    QCOMPARE(ctx->proxyConfig->port, static_cast<quint16>(8888));
    QVERIFY(ctx->downloadConfig != nullptr);
    QCOMPARE(ctx->downloadConfig->threadCount, static_cast<quint16>(4));
    QVERIFY(ctx->uploadConfig != nullptr);
    QVERIFY(ctx->uploadConfig->usePutMethod);
    QCOMPARE(ctx->userContext.toString(), QString("ctx-token"));
#ifndef QT_NO_SSL
    QVERIFY(ctx->sslConfig != nullptr);
    QVERIFY(ctx->sslConfig->peerVerifyMode == SslConfig::PeerVerifyMode::VerifyNone);
#endif
}

void TestRequestBuilder::testBuildConsumesBuilder()
{
    // build() is single-shot: it hands out ownership via std::exchange(...,nullptr).
    RequestContextBuilder builder;
    builder.url("http://example.com").type(RequestType::Get);
    auto first = builder.build();
    QVERIFY(first != nullptr);

    auto second = builder.build();
    QVERIFY(second == nullptr);
}

void TestRequestBuilder::testQueryParamBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com/api")
        .type(RequestType::Get)
        .queryParam("page", "1")
        .queryParam("limit", "50")
        .build();

    QCOMPARE(ctx->queryParams.size(), 2);
    QCOMPARE(ctx->queryParams["page"], QString("1"));
    QCOMPARE(ctx->queryParams["limit"], QString("50"));
}

void TestRequestBuilder::testQueryParamsMapBuilder()
{
    QMap<QString, QString> params;
    params["sort"] = "desc";
    params["filter"] = "active";

    auto ctx = RequestContextBuilder()
        .url("https://example.com/api")
        .type(RequestType::Get)
        .queryParams(params)
        .build();

    QCOMPARE(ctx->queryParams.size(), 2);
    QCOMPARE(ctx->queryParams["sort"], QString("desc"));
    QCOMPARE(ctx->queryParams["filter"], QString("active"));
}

void TestRequestBuilder::testAuthBuilder()
{
    auto ctx = RequestContextBuilder()
        .url("https://example.com/api")
        .type(RequestType::Get)
        .authBearer("test-token")
        .build();

    QVERIFY(ctx->authConfig.type == AuthType::Bearer);
    QCOMPARE(ctx->authConfig.token, QString("test-token"));

    // Verify builder can also set via AuthConfig directly
    auto ctx2 = RequestContextBuilder()
        .url("https://example.com/api")
        .type(RequestType::Get)
        .authConfig(AuthConfig::basic("admin", "123"))
        .build();

    QVERIFY(ctx2->authConfig.type == AuthType::Basic);
    QCOMPARE(ctx2->authConfig.username, QString("admin"));
}

void TestRequestBuilder::testBodyFormDataBuilder()
{
    QStringList files = {QStringLiteral("/tmp/a.png"), QStringLiteral("/tmp/b.zip")};
    QMap<QString, QString> kv;
    kv["field1"] = "value1";
    kv["field2"] = "value2";

    auto ctx = RequestContextBuilder()
        .url("https://example.com/upload")
        .type(RequestType::Post)
        .bodyFormData(files, kv)
        .build();

    // bodyType must be symmetric with the FormData enum value.
    QVERIFY(ctx->bodyType == BodyType::FormData);
    // uploadConfig carries the multipart parts.
    QVERIFY(ctx->uploadConfig != nullptr);
    QVERIFY(ctx->uploadConfig->useFormData);
    QCOMPARE(ctx->uploadConfig->files, files);
    QCOMPARE(ctx->uploadConfig->kvPairs, kv);
    // No automatic Content-Type is set for FormData (boundary is added by the
    // pipeline), so the body string stays empty here.
    QVERIFY(ctx->body.isEmpty());
}
