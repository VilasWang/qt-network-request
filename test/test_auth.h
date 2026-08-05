#pragma once

#include <QObject>
#include <QSharedPointer>
#include "requestcontext.h"
#include "networkrequestmanager.h"
#include "networkreply.h"
#include "httptestserver.h"

class AuthTests : public QObject
{
    Q_OBJECT

public:
    AuthTests();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // No-network builder validation tests
    void testAuthBasicBuilder();
    void testAuthBearerBuilder();
    void testAuthApiKeyHeaderBuilder();
    void testAuthApiKeyQueryBuilder();
    void testAuthConfigIsValid();
    void testAuthBasicHeaderValue();
    void testAuthBearerHeaderValue();
    void testAuthNoneNoHeader();

    // Network integration tests
    void testAuthBasicRequest();
    void testAuthBearerRequest();
    void testAuthApiKeyHeaderRequest();
    void testAuthApiKeyQueryRequest();
    void testAuthHeaderPriority();
    void testAuthInvalidCredentials();

    // OAuth2 integration tests (M2)
    void testOAuth2ClientCredentials();
    void testOAuth2ClientCredentialsCache();
    void testOAuth2PasswordGrant();
    void testOAuth2RefreshToken();
    void testOAuth2TokenExpired();

private:
    void waitForResponse(int timeoutMs = 5000);

    QtNetworkRequest::NetworkRequestManager *m_manager{ nullptr };
    QSharedPointer<QtNetworkRequest::ResponseResult> m_lastResult;
    bool m_responseReceived{ false };
};
