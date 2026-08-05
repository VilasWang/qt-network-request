#include "test_environment.h"

#include "requestcontext.h"
#include "authconfig.h"
#include "environment.h"

using namespace QtNetworkRequest;

void EnvironmentTests::testSubstituteBasic()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://example.com";
    QCOMPARE(substituteEnv("{{host}}/api", vars), QString("http://example.com/api"));
    // multiple occurrences
    QCOMPARE(substituteEnv("{{host}}/a/{{host}}/b", vars),
             QString("http://example.com/a/http://example.com/b"));
}

void EnvironmentTests::testSubstituteMissingLeftIntact()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://example.com";
    // unknown key is left verbatim (Postman-style)
    QCOMPARE(substituteEnv("{{missing}}/api", vars), QString("{{missing}}/api"));
    // no vars at all -> unchanged
    QCOMPARE(substituteEnv("{{host}}/api", QMap<QString, QString>()), QString("{{host}}/api"));
    // unbalanced braces -> left intact
    QCOMPARE(substituteEnv("{{host", vars), QString("{{host"));
}

void EnvironmentTests::testSubstituteUrlAndQueryParams()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://h";
    vars["q"]    = "search";

    auto ctx = RequestContextBuilder()
                   .url("{{host}}/x")
                   .queryParam("q", "{{q}}")
                   .build();

    applyEnvironment(*ctx, vars);
    QCOMPARE(ctx->url, QString("http://h/x"));
    QCOMPARE(ctx->queryParams.value("q"), QString("search"));
}

void EnvironmentTests::testSubstituteHeaders()
{
    QMap<QString, QString> vars;
    vars["tok"] = "abc";

    auto ctx = RequestContextBuilder()
                   .header("X-Token", "{{tok}}")
                   .build();

    applyEnvironment(*ctx, vars);
    QCOMPARE(QString::fromUtf8(ctx->headers.value("X-Token")), QString("abc"));
}

void EnvironmentTests::testSubstituteBody()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://h";

    auto ctx = RequestContextBuilder()
                   .bodyJson("{\"url\":\"{{host}}\"}")
                   .build();

    applyEnvironment(*ctx, vars);
    QCOMPARE(ctx->body, QString("{\"url\":\"http://h\"}"));
}

void EnvironmentTests::testSubstituteSkipsBinary()
{
    QByteArray bin;
    bin.append('A');
    bin.append('B');
    bin.append('\0');   // embedded NUL must survive untouched
    bin.append('C');
    bin.append('D');

    auto ctx = RequestContextBuilder().bodyBinary(bin).build();

    QMap<QString, QString> vars;
    vars["x"] = "y";
    applyEnvironment(*ctx, vars);

    QCOMPARE(ctx->binaryBody, bin);   // never mutated
}

void EnvironmentTests::testSubstituteAuthFields()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://h";
    vars["cid"]  = "client-123";

    // OAuth2 tokenUrl / clientId carry {{var}} (R3)
    AuthConfig::OAuth2Config o;
    o.grant     = OAuth2GrantType::ClientCredentials;
    o.tokenUrl  = "{{host}}/oauth/token";
    o.clientId  = "{{cid}}";
    o.clientSecret = "secret";

    auto ctx = RequestContextBuilder().authConfig(AuthConfig::oauth2(o)).build();
    applyEnvironment(*ctx, vars);

    QCOMPARE(ctx->authConfig.oauth2Config.tokenUrl, QString("http://h/oauth/token"));
    QCOMPARE(ctx->authConfig.oauth2Config.clientId, QString("client-123"));
    QCOMPARE(ctx->authConfig.oauth2Config.clientSecret, QString("secret"));

    // Bearer token also substituted
    auto ctx2 = RequestContextBuilder().authConfig(AuthConfig::bearer("{{tok}}")).build();
    QMap<QString, QString> vars2;
    vars2["tok"] = "secret-token";
    applyEnvironment(*ctx2, vars2);
    QCOMPARE(ctx2->authConfig.token, QString("secret-token"));
}

void EnvironmentTests::testRequestContextBuilderEnvironment()
{
    QMap<QString, QString> vars;
    vars["host"] = "http://t";

    auto ctx = RequestContextBuilder()
                   .url("{{host}}/x")
                   .environment(vars)
                   .build();

    // The pipeline applies ctx->environment; emulate that here.
    applyEnvironment(*ctx, ctx->environment);
    QCOMPARE(ctx->url, QString("http://t/x"));
}
