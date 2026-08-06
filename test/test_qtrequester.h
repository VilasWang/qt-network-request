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

    // ---- Stage D: comprehensive real-user UI interaction coverage ----

    // Request history: save / replay / search / empty + overflow boundary
    void testHistorySaveAndReplay();
    void testHistorySearchFilter();
    void testHistoryEmptyAndOverflow();

    // Environment variable {{var}} expansion in URL (mock-verified)
    void testEnvVariableExpansionInUrl();

    // Auth: OAuth2 all grants + ApiKey query injection + empty-credential fallback
    void testBuildAuthConfigOAuth2AllGrants();
    void testApiKeyQueryInjection();
    void testAuthEmptyCredentialFallback();

    // Response formatting: JSON pretty/raw toggle, XML/HTML display, malformed JSON tolerance
    void testResponsePrettyToggle();
    void testResponseXmlHtmlDisplay();
    void testResponseMalformedJsonTolerance();

    // Collection management: CRUD + tree replay + Postman v2.1 round-trip + special chars
    void testCollectionCrudAndReplay();
    void testPostmanImportExportRoundTrip();

    // Exception / boundary: invalid URL, empty send, timeout, abort
    void testInvalidUrlSendHandling();
    void testAbortRunningRequest();
    void testTransferTimeoutHandling();

    // Batch execution (UI not yet implemented) — placeholder
    void testBatchExecutionPlaceholder();
};

#endif
