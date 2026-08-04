#pragma once

#include <QObject>
#include <QSharedPointer>
#include "requestcontext.h"
#include "networkrequestmanager.h"
#include "networkreply.h"
#include "httptestserver.h"

class BodyTypeTests : public QObject
{
    Q_OBJECT

public:
    BodyTypeTests();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // No-network builder validation tests
    void testBodyJsonBuilder();
    void testBodyXmlBuilder();
    void testBodyFormUrlEncodedBuilder();
    void testBodyRawBuilder();
    void testBodyBinaryBuilder();
    void testBodyTypeDefault();

    // Network integration tests
    void testBodyJsonContentType();
    void testBodyXmlContentType();
    void testBodyFormUrlEncodedContentType();
    void testBodyBinaryRequest();
    void testBodyCTPriority();

private:
    void waitForResponse(int timeoutMs = 5000);

    QtNetworkRequest::NetworkRequestManager *m_manager{ nullptr };
    QSharedPointer<QtNetworkRequest::ResponseResult> m_lastResult;
    bool m_responseReceived{ false };
};
