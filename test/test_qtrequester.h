#ifndef TEST_QTREQUESTER_H
#define TEST_QTREQUESTER_H

#include <QObject>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "httptestserver.h"

namespace QtNetworkRequest {
class NetworkRequestTool;
}

class TestQtRequester : public QObject
{
    Q_OBJECT

private slots:
    void testBuildUrlWithParams();
    void testGetHeaders();
    void testGetRequestBody();
    void testMethodSwitchesBody();
    void testApplyAuthHeader();
    void testBuildAuthConfigApiKey();
    void testBuildRequestContextQueryParams();
    void testBuildRequestContextBinary();
    void testBuildRequestContextAuth();
    void testSaveAndLoadDisk();
    void testEndToEndGet();

    // Stage C: additional pure NetworkRequestTool helpers + syntax highlighters.
    void testBytesToString();
    void testGetRequestTypeMapping();
    void testContentTypeDetection();
    void testFormatDateTime();
    void testUpdateBodyTypeFromContentType();
    void testJsonHighlighter();
    void testXmlHighlighter();

    // M1 environment
    void test_envDropdownSwitch();

    // M4 auth save/load + M3 search
    void testSaveAndLoadDiskWithAuth();
    void testResponseSearch();
};

#endif
