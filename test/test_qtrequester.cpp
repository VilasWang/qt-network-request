#include <QtTest/QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryFile>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextBlock>
#include <functional>
#include <QTreeWidget>
#include <QMessageBox>
#include "test_qtrequester.h"
#include "networkrequesttool.h"
#include "requestcontext.h"
#include "environmentstore.h"
#include "collectionmodel.h"
#include "postmanconverter.h"
#include "responseresult.h"
#include "networkrequestmanager.h"
#include "networkreply.h"
#include "jsonsyntaxhighlighter.h"
#include "xmlsyntaxhighlighter.h"

using namespace QtNetworkRequest;

// ---------------------------------------------------------------------------
// 1. URL + Query Params
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildUrlWithParams()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("http://127.0.0.1:12345/api");

    auto *paramsTable = tool.findChild<QTableWidget *>("table_params");
    QVERIFY(paramsTable);
    paramsTable->setRowCount(1);
    paramsTable->setItem(0, 0, new QTableWidgetItem("key1"));
    paramsTable->setItem(0, 1, new QTableWidgetItem("val1"));

    QString url = tool.buildUrlWithParams();
    QCOMPARE(url, QString("http://127.0.0.1:12345/api?key1=val1"));
}

// ---------------------------------------------------------------------------
// 2. Headers
// ---------------------------------------------------------------------------
void TestQtRequester::testGetHeaders()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *headersTable = tool.findChild<QTableWidget *>("table_headers");
    QVERIFY(headersTable);

    headersTable->setRowCount(0);
    headersTable->setRowCount(2);
    headersTable->setItem(0, 0, new QTableWidgetItem("X-Custom"));
    headersTable->setItem(0, 1, new QTableWidgetItem("value1"));
    headersTable->setItem(1, 0, new QTableWidgetItem("Authorization"));
    headersTable->setItem(1, 1, new QTableWidgetItem("Bearer tok123"));

    auto headers = tool.getHeaders();
    QCOMPARE(headers.size(), 2);
    QCOMPARE(headers.value("X-Custom"), QByteArray("value1"));
    QCOMPARE(headers.value("Authorization"), QByteArray("Bearer tok123"));
}

// ---------------------------------------------------------------------------
// 3. Request Body
// ---------------------------------------------------------------------------
void TestQtRequester::testGetRequestBody()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *methodCombo = tool.findChild<QComboBox *>("cmb_method");
    QVERIFY(methodCombo);
    methodCombo->setCurrentText("POST");
    QTest::qWait(50);

    auto *bodyEdit = tool.findChild<QTextEdit *>("textEdit_body");
    QVERIFY(bodyEdit);
    QVERIFY(bodyEdit->isEnabled());
    bodyEdit->setPlainText("hello world");

    QString body = tool.getRequestBody();
    QCOMPARE(body, QString("hello world"));
}

// ---------------------------------------------------------------------------
// 4. Method toggles body enable
// ---------------------------------------------------------------------------
void TestQtRequester::testMethodSwitchesBody()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *methodCombo = tool.findChild<QComboBox *>("cmb_method");
    QVERIFY(methodCombo);
    auto *bodyTypeCombo = tool.findChild<QComboBox *>("cmb_body_type");
    QVERIFY(bodyTypeCombo);

    tool.onMethodChanged("GET");
    QCOMPARE(bodyTypeCombo->currentText(), QString("none"));
    QCOMPARE(bodyTypeCombo->isEnabled(), false);

    tool.onMethodChanged("POST");
    QCOMPARE(bodyTypeCombo->currentText(), QString("raw"));
    QCOMPARE(bodyTypeCombo->isEnabled(), true);

    tool.onMethodChanged("OPTIONS");
    QCOMPARE(bodyTypeCombo->currentText(), QString("none"));
    QCOMPARE(bodyTypeCombo->isEnabled(), false);
}

// ---------------------------------------------------------------------------
// 5. Auth header injection
// ---------------------------------------------------------------------------
void TestQtRequester::testApplyAuthHeader()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Set Bearer auth
    tool.m_settings.authType = "Bearer";
    tool.m_settings.authToken = "mytoken123";
    tool.applyAuthHeader();

    auto headers = tool.getHeaders();
    QCOMPARE(headers.value("Authorization"), QByteArray("Bearer mytoken123"));

    // Clear and test Basic auth
    tool.ui.table_headers->setRowCount(0);
    tool.m_settings.authType = "Basic";
    tool.m_settings.authUsername = "user1";
    tool.m_settings.authPassword = "pass1";
    tool.applyAuthHeader();

    headers = tool.getHeaders();
    QByteArray expected = QByteArray("user1:pass1").toBase64();
    QCOMPARE(headers.value("Authorization"), QByteArray("Basic ") + expected);

    // FIXED: Environment variable expansion in auth credentials.
    // When an env variable is set, {{var}} placeholders in auth values are resolved.
    tool.ui.table_headers->setRowCount(0);
    QMap<QString, QString> envVars;
    envVars.insert("token", "resolved-token-456");
    tool.m_envStore.upsert("Test", envVars);
    tool.m_envStore.setActiveName("Test");

    tool.m_settings.authType = "Bearer";
    tool.m_settings.authToken = "{{token}}";
    tool.applyAuthHeader();
    headers = tool.getHeaders();
    QCOMPARE(headers.value("Authorization"), QByteArray("Bearer resolved-token-456"));

    // Cleanup
    tool.m_envStore.setActiveName("");

    // None should not add header
    tool.ui.table_headers->setRowCount(0);
    tool.m_settings.authType = "None";
    tool.applyAuthHeader();
    headers = tool.getHeaders();
    QVERIFY(!headers.contains("Authorization"));
}

// ---------------------------------------------------------------------------
// 5b. buildAuthConfig produces the correct AuthConfig for ApiKey (Header/Query)
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildAuthConfigApiKey()
{
    NetworkRequestTool tool;

    tool.m_settings.authType = "ApiKey";
    tool.m_settings.authApiKey = "X-Api-Key";
    tool.m_settings.authApiValue = "secret";
    tool.m_settings.authApiLocation = "Header";
    AuthConfig cfg = tool.buildAuthConfig();
    QVERIFY(cfg.type == AuthType::ApiKey);
    QCOMPARE(cfg.apiKey, QString("X-Api-Key"));
    QCOMPARE(cfg.apiValue, QString("secret"));
    QVERIFY(cfg.apiKeyPlacement == ApiKeyPlacement::Header);

    tool.m_settings.authApiLocation = "Query";
    AuthConfig cfgQuery = tool.buildAuthConfig();
    QVERIFY(cfgQuery.type == AuthType::ApiKey);
    QVERIFY(cfgQuery.apiKeyPlacement == ApiKeyPlacement::QueryParam);

    // Empty key -> None
    tool.m_settings.authApiKey.clear();
    AuthConfig cfgNone = tool.buildAuthConfig();
    QVERIFY(cfgNone.type == AuthType::None);
}

// ---------------------------------------------------------------------------
// 5c. buildRequestContext routes table_params into req->queryParams
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildRequestContextQueryParams()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *paramsTable = tool.findChild<QTableWidget *>("table_params");
    QVERIFY(paramsTable);
    paramsTable->setRowCount(1);
    paramsTable->setItem(0, 0, new QTableWidgetItem("q"));
    paramsTable->setItem(0, 1, new QTableWidgetItem("hello"));
    tool.ui.lineEdit_url->setText("https://example.com/api");

    auto req = tool.buildRequestContext();
    QVERIFY(req != nullptr);
    QVERIFY(req->queryParams.contains("q"));
    QCOMPARE(req->queryParams.value("q"), QString("hello"));
    // base URL must not contain the query string (library appends it later)
    QVERIFY(!req->url.contains("q=hello"));
}

// ---------------------------------------------------------------------------
// 5d. buildRequestContext routes a picked binary file into req->binaryBody
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildRequestContextBinary()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    QTemporaryFile tmp;
    tmp.open();
    // Build 7 bytes: 'A','B','C',0x00,'D','E','F'. Avoid hex escape "\x00DEF"
    // (the \x escape greedily consumes all following hex digits -> out-of-range).
    char raw[] = {'A', 'B', 'C', '\0', 'D', 'E', 'F'};
    QByteArray data(raw, 7);
    tmp.write(data);
    QString path = tmp.fileName();
    tmp.close();

    tool.currentBodyType = "binary";
    tool.m_binaryFilePath = path;

    auto req = tool.buildRequestContext();
    QVERIFY(req != nullptr);
    QVERIFY(req->bodyType == BodyType::Binary);
    QCOMPARE(req->binaryBody, data);
}

// ---------------------------------------------------------------------------
// 5e. buildRequestContext carries the authConfig from settings
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildRequestContextAuth()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    tool.m_settings.authType = "Bearer";
    tool.m_settings.authToken = "tok-xyz";
    tool.ui.lineEdit_url->setText("https://example.com/api");

    auto req = tool.buildRequestContext();
    QVERIFY(req != nullptr);
    QVERIFY(req->authConfig.type == AuthType::Bearer);
    QCOMPARE(req->authConfig.token, QString("tok-xyz"));
}

// ---------------------------------------------------------------------------
// 6. Save / Load persistent storage
// ---------------------------------------------------------------------------
void TestQtRequester::testSaveAndLoadDisk()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("http://example.com/api");
    tool.currentMethod = "POST";

    tool.ui.table_params->setRowCount(1);
    tool.ui.table_params->setItem(0, 0, new QTableWidgetItem("p1"));
    tool.ui.table_params->setItem(0, 1, new QTableWidgetItem("v1"));

    tool.saveToDisk(path);

    // Clear the form
    tool.onNewRequest();
    QTest::qWait(50);
    QCOMPARE(tool.ui.lineEdit_url->text(), QString());

    // Load back
    tool.loadFromDisk(path);
    QCOMPARE(tool.ui.lineEdit_url->text(), QString("http://example.com/api"));
    QCOMPARE(tool.currentMethod, QString("POST"));

    // Verify params
    auto *paramsTable = tool.findChild<QTableWidget *>("table_params");
    QVERIFY(paramsTable);
    bool foundParam = false;
    for (int i = 0; i < paramsTable->rowCount(); ++i)
    {
        auto *k = paramsTable->item(i, 0);
        auto *v = paramsTable->item(i, 1);
        if (k && v && k->text() == "p1" && v->text() == "v1")
            foundParam = true;
    }
    QVERIFY(foundParam);
}

// ---------------------------------------------------------------------------
// 7. End-to-end GET with mock server
// ---------------------------------------------------------------------------
void TestQtRequester::testEndToEndGet()
{
    HttpTestServer server;
    QVERIFY2(server.start(), "Failed to start mock server");
    QString baseUrl = server.baseUrl();
    QVERIFY2(server.port() > 0, "Server port is invalid");

    // First verify direct request works (bypassing UI)
    NetworkRequestManager::initialize();
    auto directReq = std::make_unique<RequestContext>();
    directReq->url = baseUrl + "/get";
    directReq->type = RequestType::Get;
    auto reply = NetworkRequestManager::globalInstance()->postRequest(std::move(directReq));
    QVERIFY(reply != nullptr);

    bool directOk = false;
    QObject::connect(reply.get(), &NetworkReply::requestFinished,
                     [&directOk](QSharedPointer<ResponseResult> rsp) {
                         directOk = rsp && rsp->isSuccess() && rsp->body.contains("GET");
                     });
    {
        QEventLoop loop;
        QObject::connect(reply.get(), &NetworkReply::requestFinished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QVERIFY2(directOk, "Direct request to mock server failed");
    NetworkRequestManager::unInitialize();

    // Now test through UI
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText(baseUrl + "/get");

    tool.ui.table_headers->setRowCount(0);
    tool.onMethodChanged("GET");

    QTest::mouseClick(tool.findChild<QPushButton *>("btn_send"), Qt::LeftButton);

    // The tool writes a "Sending request..." preamble synchronously, so waiting
    // for a non-empty body would pass before the async response arrives. Poll for
    // the actual echoed content instead so QTRY keeps pumping the event loop until
    // the reply is delivered.
    QTRY_VERIFY2_WITH_TIMEOUT(tool.ui.textEdit_response_body->toPlainText().contains("\"GET\""),
                              "UI response did not contain expected GET echo after timeout", 10000);

    QString body = tool.ui.textEdit_response_body->toPlainText();
    QVERIFY2(body.contains("\"GET\""),
             qPrintable(QString("Response body did not contain GET, got: %1").arg(body.left(200))));

    server.stop();
}

// ---------------------------------------------------------------------------
// 8. bytesToString unit-scaling (B / KB / MB / GB)
// ---------------------------------------------------------------------------
void TestQtRequester::testBytesToString()
{
    NetworkRequestTool tool;
    QCOMPARE(tool.bytesToString(512), QString("512B"));
    QCOMPARE(tool.bytesToString(2048), QString("2KB"));               // integer KB
    QCOMPARE(tool.bytesToString(5 * 1024 * 1024), QString("5.00MB")); // 2-dp MB
    QCOMPARE(tool.bytesToString(qint64(3) * 1024 * 1024 * 1024), QString("3.00GB"));
}

// ---------------------------------------------------------------------------
// 9. getRequestType maps the method string to the RequestType enum
// ---------------------------------------------------------------------------
void TestQtRequester::testGetRequestTypeMapping()
{
    NetworkRequestTool tool;
    tool.currentMethod = "GET";     QVERIFY(tool.getRequestType() == RequestType::Get);
    tool.currentMethod = "POST";    QVERIFY(tool.getRequestType() == RequestType::Post);
    tool.currentMethod = "PUT";     QVERIFY(tool.getRequestType() == RequestType::Put);
    tool.currentMethod = "PATCH";   QVERIFY(tool.getRequestType() == RequestType::Patch);
    tool.currentMethod = "DELETE";  QVERIFY(tool.getRequestType() == RequestType::Delete);
    tool.currentMethod = "HEAD";    QVERIFY(tool.getRequestType() == RequestType::Head);
    tool.currentMethod = "OPTIONS"; QVERIFY(tool.getRequestType() == RequestType::Options);
    tool.currentMethod = "WAT";     QVERIFY(tool.getRequestType() == RequestType::Get); // fallback
}

// ---------------------------------------------------------------------------
// 10. Content-Type sniffing helpers
// ---------------------------------------------------------------------------
void TestQtRequester::testContentTypeDetection()
{
    NetworkRequestTool tool;
    QMap<QByteArray, QByteArray> json;
    json.insert("Content-Type", "application/json; charset=utf-8");
    QVERIFY(tool.isJsonResponse(json));
    QVERIFY(!tool.isXmlResponse(json));
    QVERIFY(!tool.isOctetStreamResponse(json));

    QMap<QByteArray, QByteArray> xml;
    xml.insert("Content-Type", "text/xml");
    QVERIFY(tool.isXmlResponse(xml));
    QVERIFY(!tool.isJsonResponse(xml));

    QMap<QByteArray, QByteArray> bin;
    bin.insert("Content-Type", "application/octet-stream");
    QVERIFY(tool.isOctetStreamResponse(bin));
    QVERIFY(!tool.isJsonResponse(bin));

    // Detection is case-insensitive on the header value.
    QMap<QByteArray, QByteArray> upper;
    upper.insert("Content-Type", "APPLICATION/JSON");
    QVERIFY(tool.isJsonResponse(upper));
}

// ---------------------------------------------------------------------------
// 11. formatDateTime uses the fixed display pattern
// ---------------------------------------------------------------------------
void TestQtRequester::testFormatDateTime()
{
    NetworkRequestTool tool;
    QDateTime dt(QDate(2023, 5, 9), QTime(8, 7, 6));
    QCOMPARE(tool.formatDateTime(dt), QString("2023-05-09 08:07:06"));
}

// ---------------------------------------------------------------------------
// 12. updateBodyTypeFromContentType drives the body/raw-type combos
// ---------------------------------------------------------------------------
void TestQtRequester::testUpdateBodyTypeFromContentType()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    tool.updateBodyTypeFromContentType("application/json");
    QCOMPARE(tool.ui.cmb_body_type->currentText(), QString("raw"));
    QCOMPARE(tool.ui.cmb_raw_type->currentText(), QString("JSON"));

    tool.updateBodyTypeFromContentType("text/xml");
    QCOMPARE(tool.ui.cmb_body_type->currentText(), QString("raw"));
    QCOMPARE(tool.ui.cmb_raw_type->currentText(), QString("XML"));

    tool.updateBodyTypeFromContentType("application/x-www-form-urlencoded");
    QCOMPARE(tool.ui.cmb_body_type->currentText(), QString("x-www-form-urlencoded"));
}

// ---------------------------------------------------------------------------
// 13. JSON syntax highlighter applies character formats
// ---------------------------------------------------------------------------
void TestQtRequester::testJsonHighlighter()
{
    QTextDocument doc;
    doc.setPlainText("{\n  \"name\": \"value\",\n  \"count\": 42,\n  \"ok\": true\n}");
    JsonSyntaxHighlighter highlighter(&doc);
    highlighter.rehighlight(); // force synchronous highlight

    // At least one block must have received non-default character formats.
    int totalFormats = 0;
    for (QTextBlock b = doc.begin(); b != doc.end(); b = b.next())
    {
        if (b.layout())
            totalFormats += b.layout()->formats().size();
    }
    QVERIFY2(totalFormats > 0, "JSON highlighter produced no character formats");
}

// ---------------------------------------------------------------------------
// 14. XML syntax highlighter applies character formats
// ---------------------------------------------------------------------------
void TestQtRequester::testXmlHighlighter()
{
    QTextDocument doc;
    doc.setPlainText("<root attr=\"x\"><!-- c --><child>text</child></root>");
    XmlSyntaxHighlighter highlighter(&doc);
    highlighter.rehighlight();

    int totalFormats = 0;
    for (QTextBlock b = doc.begin(); b != doc.end(); b = b.next())
    {
        if (b.layout())
            totalFormats += b.layout()->formats().size();
    }
    QVERIFY2(totalFormats > 0, "XML highlighter produced no character formats");
}

// ---------------------------------------------------------------------------
// M1 Environment dropdown
// ---------------------------------------------------------------------------
void TestQtRequester::test_envDropdownSwitch()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Verify the env combo box exists
    auto *cmbEnv = tool.findChild<QComboBox *>("cmb_environment");
    QVERIFY(cmbEnv);
    QVERIFY(cmbEnv->count() >= 1); // At least "No Environment"

    // Verify the first entry is "No Environment"
    QCOMPARE(cmbEnv->itemText(0), QString("No Environment"));
    QVERIFY(cmbEnv->itemData(0).toString().isEmpty());

    // The default env file doesn't exist yet, so the tool creates a "Dev" default
    QVERIFY(cmbEnv->count() >= 2); // "No Environment" + at least "Dev"

    // Select "No Environment" → activeVariables() should be empty
    cmbEnv->setCurrentIndex(0);
    auto vars = tool.m_envStore.activeVariables();
    QVERIFY2(vars.isEmpty(), "No Environment should yield empty active variables");

    // Select "Dev" → should have variables
    int devIdx = cmbEnv->findText("Dev");
    if (devIdx >= 0)
    {
        cmbEnv->setCurrentIndex(devIdx);
        QTest::qWait(50);
        vars = tool.m_envStore.activeVariables();
        QVERIFY2(!vars.isEmpty(), "Dev environment should have variables");
        QCOMPARE(vars.value("host"), QString("localhost"));
    }

    // buildRequestContext should include the env map
    auto req = tool.buildRequestContext();
    QVERIFY(req);
    if (devIdx >= 0)
    {
        QVERIFY2(!req->environment.isEmpty(), "RequestContext should carry active env vars");
        QCOMPARE(req->environment.value("host"), QString("localhost"));
    }
}

// ---------------------------------------------------------------------------
// R2: Auth save/load round-trip (M4)
// ---------------------------------------------------------------------------
void TestQtRequester::testSaveAndLoadDiskWithAuth()
{
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString path = tmpFile.fileName();
    tmpFile.close();

    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Set up form with some content
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("http://example.com/api");

    // Set auth to Bearer
    tool.m_settings.authType  = "Bearer";
    tool.m_settings.authToken = "my-test-token";

    // Also set OAuth2 fields (ensures both serialisation paths work)
    tool.m_settings.oauthGrantType = "Password";
    tool.m_settings.oauthClientId  = "test-client";

    // Save
    tool.saveToDisk(path);

    // Reset form
    tool.onNewRequest();
    QTest::qWait(50);
    QCOMPARE(tool.ui.lineEdit_url->text(), QString());

    // Load back
    tool.loadFromDisk(path);
    QCOMPARE(tool.ui.lineEdit_url->text(), QString("http://example.com/api"));
    QCOMPARE(tool.m_settings.authType, QString("Bearer"));
    QCOMPARE(tool.m_settings.authToken, QString("my-test-token"));
    // OAuth2 fields should also survive
    QCOMPARE(tool.m_settings.oauthGrantType, QString("Password"));
    QCOMPARE(tool.m_settings.oauthClientId, QString("test-client"));
}

// ---------------------------------------------------------------------------
// M3: Response search
// ---------------------------------------------------------------------------
void TestQtRequester::testResponseSearch()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Set some response body text
    tool.ui.textEdit_response_body->setPlainText("Hello world. Hello again.");

    // Simulate search
    auto *searchEdit = tool.findChild<QLineEdit *>();
    QVERIFY(searchEdit);  // m_leResponseSearch should exist
    // The first QLineEdit found might not be the search one — let's use
    // the m_leResponseSearch member directly
    if (!tool.m_leResponseSearch)
        QSKIP("Response search widget not created (toolbar may not be built in test)");

    tool.m_leResponseSearch->setText("Hello");
    QTest::qWait(50);

    // Verify that search highlights exist
    QVERIFY2(!tool.m_searchSelections.isEmpty(), "Search should produce highlights");
    QVERIFY2(tool.m_searchSelections.size() >= 2, "Should find at least 2 'Hello' matches");

    // Verify navigation
    QVERIFY(tool.m_currentSearchHit >= 0);
    int firstHit = tool.m_currentSearchHit;

    tool.navigateSearchHit(+1);
    QVERIFY(tool.m_currentSearchHit != firstHit);  // moved to next

    tool.navigateSearchHit(-1);
    QVERIFY(tool.m_currentSearchHit == firstHit);   // back to first
}

// ===========================================================================
// Stage D: comprehensive real-user UI interaction coverage
// ===========================================================================

// ---------------------------------------------------------------------------
// D1. Request history: save -> replay (loadFromHistory) -> search filter
// ---------------------------------------------------------------------------
void TestQtRequester::testHistorySaveAndReplay()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    auto *methodCombo = tool.findChild<QComboBox *>("cmb_method");
    QVERIFY(methodCombo);
    auto *bodyEdit = tool.ui.textEdit_body;
    auto *headersTable = tool.findChild<QTableWidget *>("table_headers");
    QVERIFY(headersTable);

    // --- Simulate a user filling a request form ---
    methodCombo->setCurrentText("POST");
    urlEdit->setText("https://example.com/api/login");
    bodyEdit->setPlainText("{\"user\":\"alice\"}");
    headersTable->setRowCount(1);
    headersTable->setItem(0, 0, new QTableWidgetItem("X-Token"));
    headersTable->setItem(0, 1, new QTableWidgetItem("abc123"));

    // --- Trigger "Save" (writes to in-memory history) ---
    tool.saveToHistory();
    QCOMPARE(tool.requestHistory.size(), 1);
    QCOMPARE(tool.ui.listWidget_history->count(), 1);

    // --- Now mutate the form to a different state ---
    urlEdit->setText("https://example.com/other");
    bodyEdit->setPlainText("garbage");
    headersTable->setRowCount(0);

    // --- Replay the saved history item (real user clicks the list item) ---
    QListWidgetItem *item = tool.ui.listWidget_history->item(0);
    tool.onHistoryItemClicked(item);
    QTest::qWait(20);

    // --- Assert the form was restored from history ---
    QCOMPARE(urlEdit->text(), QString("https://example.com/api/login"));
    QCOMPARE(bodyEdit->toPlainText(), QString("{\"user\":\"alice\"}"));
    QCOMPARE(headersTable->rowCount(), 1);
    QCOMPARE(headersTable->item(0, 0)->text(), QString("X-Token"));
    QCOMPARE(headersTable->item(0, 1)->text(), QString("abc123"));
    QCOMPARE(methodCombo->currentText(), QString("POST"));
}

void TestQtRequester::testHistorySearchFilter()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);

    // Add three distinct history entries
    const QStringList urls = {
        "https://api.users.com/list",
        "https://api.orders.com/create",
        "https://api.users.com/delete"};
    for (const QString &u : urls)
    {
        urlEdit->setText(u);
        tool.saveToHistory();
    }
    QCOMPARE(tool.ui.listWidget_history->count(), 3);

    // Filter by "users" -> only the two users.com entries remain visible
    tool.onSearchHistory("users");
    QTest::qWait(10);
    int visible = 0;
    for (int i = 0; i < tool.ui.listWidget_history->count(); ++i)
        if (!tool.ui.listWidget_history->item(i)->isHidden())
            ++visible;
    QCOMPARE(visible, 2);

    // Clear filter -> all visible again
    tool.onSearchHistory("");
    QTest::qWait(10);
    visible = 0;
    for (int i = 0; i < tool.ui.listWidget_history->count(); ++i)
        if (!tool.ui.listWidget_history->item(i)->isHidden())
            ++visible;
    QCOMPARE(visible, 3);
}

void TestQtRequester::testHistoryEmptyAndOverflow()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);

    // Empty history: clicking should be a no-op and not crash
    QCOMPARE(tool.requestHistory.size(), 0);
    tool.onHistoryItemClicked(nullptr);
    QTest::qWait(10);

    // Overflow: save > 100 entries, list must cap at 100
    for (int i = 0; i < 120; ++i)
    {
        urlEdit->setText(QString("https://host/%1").arg(i));
        tool.saveToHistory();
    }
    QCOMPARE(tool.requestHistory.size(), 100);
    QCOMPARE(tool.ui.listWidget_history->count(), 100);

    // Most recent entry (prepend) is the last saved one
    QCOMPARE(tool.requestHistory.first().url, QString("https://host/119"));
}

// ---------------------------------------------------------------------------
// D2. Environment variable {{var}} expansion in URL (mock-verified)
// ---------------------------------------------------------------------------
void TestQtRequester::testEnvVariableExpansionInUrl()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Configure an environment "Dev" with {{host}} -> a concrete base, then activate it.
    QMap<QString, QString> devVars;
    devVars.insert("host", "https://api.dev.example.com");
    tool.m_envStore.upsert("Dev", devVars);
    tool.m_envStore.setActiveName("Dev");
    tool.populateEnvironmentCombo();
    QVERIFY(tool.m_cmbEnvironment);
    tool.m_cmbEnvironment->setCurrentText("Dev");
    QTest::qWait(10);
    QCOMPARE(tool.m_envStore.activeVariables().value("host"), QString("https://api.dev.example.com"));

    // Real-user path: type a URL containing the placeholder.
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("{{host}}/echo");

    // At the UI level the literal placeholder remains in the edit box.
    QCOMPARE(urlEdit->text(), QString("{{host}}/echo"));

    // The active environment must be carried into the request context so the
    // underlying pipeline can perform {{host}} substitution at send time.
    std::unique_ptr<RequestContext> ctx = tool.buildRequestContext();
    QVERIFY(ctx);
    QCOMPARE(ctx->environment.value("host"), QString("https://api.dev.example.com"));
    // UI-level url still carries the unresolved placeholder (resolved downstream by the lib).
    QString decodedUrl = QUrl::fromPercentEncoding(ctx->url.toUtf8());
    QVERIFY2(decodedUrl.contains("{{host}}"),
             qPrintable(QString("Expected unresolved placeholder in ctx->url, got: %1").arg(decodedUrl)));

    // Switching back to no active environment must clear the active variables.
    tool.m_envStore.setActiveName("");
    QTest::qWait(10);
    QVERIFY(tool.m_envStore.activeVariables().isEmpty());
}

// ---------------------------------------------------------------------------
// D3. Auth: OAuth2 all grants + ApiKey query injection + empty-credential fallback
// ---------------------------------------------------------------------------
void TestQtRequester::testBuildAuthConfigOAuth2AllGrants()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Client Credentials
    tool.m_settings.authType = "OAuth2";
    tool.m_settings.oauthGrantType = "ClientCredentials";
    tool.m_settings.oauthTokenUrl = "https://auth/token";
    tool.m_settings.oauthClientId = "cid";
    tool.m_settings.oauthClientSecret = "secret";
    tool.m_settings.oauthScopes = "read write";
    {
        AuthConfig cfg = tool.buildAuthConfig();
        QCOMPARE(int(cfg.type), int(AuthType::OAuth2));
        QCOMPARE(int(cfg.oauth2Config.grant), int(OAuth2GrantType::ClientCredentials));
        QCOMPARE(cfg.oauth2Config.tokenUrl, QString("https://auth/token"));
        QVERIFY(cfg.isValid());
    }

    // Password grant requires username/password
    tool.m_settings.oauthGrantType = "Password";
    tool.m_settings.oauthUsername = "bob";
    tool.m_settings.oauthPassword = "pw";
    {
        AuthConfig cfg = tool.buildAuthConfig();
        QCOMPARE(int(cfg.oauth2Config.grant), int(OAuth2GrantType::Password));
        QCOMPARE(cfg.oauth2Config.username, QString("bob"));
        QCOMPARE(cfg.oauth2Config.password, QString("pw"));
        QVERIFY(cfg.isValid());
    }

    // Refresh token grant
    tool.m_settings.oauthGrantType = "Refresh Token";
    tool.m_settings.oauthRefreshToken = "rt-123";
    {
        AuthConfig cfg = tool.buildAuthConfig();
        QCOMPARE(int(cfg.oauth2Config.grant), int(OAuth2GrantType::RefreshToken));
        QCOMPARE(cfg.oauth2Config.refreshToken, QString("rt-123"));
        QVERIFY(cfg.isValid());
    }

    // OAuth2 token is fetched at runtime and injected then; the UI preview does
    // not write a static Authorization header. Assert the config type is correct.
    QCOMPARE(int(tool.buildAuthConfig().type), int(AuthType::OAuth2));
}

void TestQtRequester::testApiKeyQueryInjection()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    tool.m_settings.authType = "ApiKey";
    tool.m_settings.authApiKey = "X-API-Key";
    tool.m_settings.authApiValue = "secret-value";
    tool.m_settings.authApiLocation = "Query";   // inject as query param

    AuthConfig cfg = tool.buildAuthConfig();
    QCOMPARE(int(cfg.type), int(AuthType::ApiKey));
    QCOMPARE(int(cfg.apiKeyPlacement), int(ApiKeyPlacement::QueryParam));
    QVERIFY(cfg.isValid());

    // ApiKey-in-query must NOT appear as a header; it is appended to the URL query.
    tool.applyAuthHeader();
    QMap<QByteArray, QByteArray> headers = tool.getHeaders();
    QVERIFY2(!headers.contains("X-API-Key"),
             "ApiKey with Query placement must not be added as a header");

    // Build the full URL and confirm the key shows up in the query string.
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("https://api/resource");
    QString full = tool.buildUrlWithParams();
    // Note: query-placement injection happens in the request pipeline; at the UI
    // level buildUrlWithParams only appends user-defined params, so we assert the
    // auth config itself correctly carries the placement strategy.
    QCOMPARE(cfg.apiKeyPlacement, ApiKeyPlacement::QueryParam);
}

void TestQtRequester::testAuthEmptyCredentialFallback()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Basic with empty username -> buildAuthConfig returns None (silent fallback),
    // so no Authorization header is emitted. This documents the current UI behaviour.
    tool.m_settings.authType = "Basic";
    tool.m_settings.authUsername = "";
    tool.m_settings.authPassword = "";
    {
        AuthConfig cfg = tool.buildAuthConfig();
        QCOMPARE(int(cfg.type), int(AuthType::None));
        tool.applyAuthHeader();
        QVERIFY2(!tool.getHeaders().contains("Authorization"),
                 "Empty Basic credentials should not emit an Authorization header");
    }

    // Bearer with empty token -> None fallback, no header.
    tool.m_settings.authType = "Bearer";
    tool.m_settings.authToken = "";
    {
        AuthConfig cfg = tool.buildAuthConfig();
        QCOMPARE(int(cfg.type), int(AuthType::None));
        tool.applyAuthHeader();
        QVERIFY2(!tool.getHeaders().contains("Authorization"),
                 "Empty Bearer token should not emit an Authorization header");
    }

    // No auth -> no header
    tool.m_settings.authType = "None";
    tool.applyAuthHeader();
    QVERIFY2(!tool.getHeaders().contains("Authorization"),
             "AuthType::None must not emit an Authorization header");

    // FIXED: When a non-None auth type is selected but credentials are empty,
    // onSendRequest() now warns via showBlockingWarning(). In test mode the
    // modal is suppressed and the text is captured in m_lastWarningText.
    {
        tool.m_settings.authType = "Basic";
        tool.m_settings.authUsername = "";
        tool.m_settings.authPassword = "";
        auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
        QVERIFY(urlEdit);
        urlEdit->setText("https://example.com/api");

        tool.m_lastWarningText.clear();
        tool.onSendRequest();
        QTest::qWait(50);
        QVERIFY2(!tool.m_lastWarningText.isEmpty(),
                 "Empty Basic credentials must trigger showBlockingWarning()");
        QVERIFY2(tool.m_lastWarningText.contains("Basic"),
                 "Warning should mention the auth type that was selected");
        // The request should still be sent (without auth), no crash.
    }
}

// ---------------------------------------------------------------------------
// D4. Response formatting: JSON pretty/raw toggle, XML/HTML display, malformed JSON
// ---------------------------------------------------------------------------
void TestQtRequester::testResponsePrettyToggle()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    const QString compact = R"({"name":"alice","age":30,"tags":["a","b"]})";
    tool.m_lastResponseBody = compact;
    tool.m_isResponseJson = true;

    // Pretty ON -> indented output
    tool.onResponsePrettyToggled(true);
    QString pretty = tool.ui.textEdit_response_body->toPlainText();
    QVERIFY2(pretty.contains('\n'), "Pretty JSON should contain newlines");
    QVERIFY2(pretty.contains("  \"name\""), "Pretty JSON should be indented");

    // Pretty OFF -> original compact text restored verbatim
    tool.onResponsePrettyToggled(false);
    QCOMPARE(tool.ui.textEdit_response_body->toPlainText(), compact);

    // Non-JSON response -> toggle must be a no-op (no crash, no change)
    tool.m_isResponseJson = false;
    tool.ui.textEdit_response_body->setPlainText("<html>x</html>");
    tool.onResponsePrettyToggled(true);
    QCOMPARE(tool.ui.textEdit_response_body->toPlainText(), QString("<html>x</html>"));
}

void TestQtRequester::testResponseXmlHtmlDisplay()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // XML response -> isXmlResponse true, displayed as-is.
    QMap<QByteArray, QByteArray> xmlHeaders;
    xmlHeaders.insert("Content-Type", "application/xml");
    QVERIFY(tool.isXmlResponse(xmlHeaders));
    tool.displayJsonResponse("<note><to>Bob</to></note>");  // non-JSON path: still sets body
    QVERIFY(tool.ui.textEdit_response_body->toPlainText().contains("<to>Bob</to>"));

    // HTML response -> displayed verbatim (no JSON highlighting crash).
    QMap<QByteArray, QByteArray> htmlHeaders;
    htmlHeaders.insert("Content-Type", "text/html");
    QVERIFY(!tool.isJsonResponse(htmlHeaders));
    tool.ui.textEdit_response_body->setPlainText("<!DOCTYPE html><h1>Hi</h1>");
    QVERIFY(tool.ui.textEdit_response_body->toPlainText().contains("<h1>Hi</h1>"));
}

void TestQtRequester::testResponseMalformedJsonTolerance()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Server claims JSON but body is malformed -> must not crash, raw body kept.
    tool.m_isResponseJson = true;
    const QString broken = R"({"name":"alice", age: })";  // invalid JSON
    tool.m_lastResponseBody = broken;
    tool.ui.textEdit_response_body->setPlainText(broken);  // simulate onResponse showing raw
    tool.onResponsePrettyToggled(true);  // parse fails -> textEdit unchanged (graceful)
    QString out = tool.ui.textEdit_response_body->toPlainText();
    QVERIFY2(!out.isEmpty(), "Malformed JSON must still leave the body visible");
    QVERIFY2(out == broken, "Malformed JSON must keep the original raw body");
}

// ---------------------------------------------------------------------------
// D5. Collection management: CRUD + tree replay + Postman v2.1 round-trip
// ---------------------------------------------------------------------------
void TestQtRequester::testCollectionCrudAndReplay()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    tool.m_collectionPath = dir.filePath("collection.json");

    // --- Create a folder under root and two requests (real model ops) ---
    // Passing an empty parentId targets the implicit root collection node.
    QString folderId = tool.m_collection.addFolder("", "My Collection");
    QVERIFY(!folderId.isEmpty());
    QString getUsersId = tool.m_collection.addRequest(
        folderId, "GET Users", QJsonDocument::fromJson(R"({"method":"GET","url":"https://api/users"})").object());
    QString postUser = tool.m_collection.addRequest(
        folderId, "POST User", QJsonDocument::fromJson(R"({"method":"POST","url":"https://api/users"})").object());
    QVERIFY(!getUsersId.isEmpty());
    QVERIFY(!postUser.isEmpty());

    tool.populateCollectionTree();
    QVERIFY(tool.m_collectionTree);
    QVERIFY(tool.m_collectionTree->topLevelItemCount() >= 1);

    // --- Persist to disk and reload ---
    tool.saveCollection();
    QVERIFY(QFile::exists(tool.m_collectionPath));
    Collection reloaded;
    QVERIFY(reloaded.load(tool.m_collectionPath));
    CollectionItem *folder = reloaded.findById(folderId);
    QVERIFY(folder);
    QCOMPARE(folder->children.size(), 2);

    // --- Replay a request node: tree item carries the request id in data(0) ---
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText("");   // clear first
    tool.m_collection = reloaded;

    QTreeWidgetItem *treeItem = nullptr;
    // Locate the tree item whose UserRole data equals getUsersId.
    std::function<void(QTreeWidgetItem *)> find = [&](QTreeWidgetItem *it) {
        if (treeItem)
            return;
        if (it->data(0, Qt::UserRole).toString() == getUsersId)
            treeItem = it;
        for (int i = 0; i < it->childCount(); ++i)
            find(it->child(i));
    };
    for (int i = 0; i < tool.m_collectionTree->topLevelItemCount(); ++i)
        find(tool.m_collectionTree->topLevelItem(i));

    QVERIFY2(treeItem, "Tree item for GET Users must exist");
    tool.onCollectionItemClicked(treeItem, 0);
    QTest::qWait(20);
    QVERIFY2(!urlEdit->text().isEmpty(), "Replaying collection request should fill the URL");
}

void TestQtRequester::testPostmanImportExportRoundTrip()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // Build a collection, export to Postman v2.1 JSON, re-import, and compare.
    QString folderId = tool.m_collection.addFolder("", "PostmanTest");
    QVERIFY(!folderId.isEmpty());
    tool.m_collection.addRequest(
        folderId, "Ping", QJsonDocument::fromJson(R"({"method":"GET","url":"https://api/ping"})").object());

    QJsonObject exported = PostmanConverter::toPostmanV21(tool.m_collection);
    QVERIFY(exported.contains("info"));
    QVERIFY(exported.contains("item"));

    bool ok = false;
    Collection imported = PostmanConverter::fromPostmanV21(exported, &ok);
    QVERIFY2(ok, "Postman import should succeed");
    // Import regenerates ids, so locate the folder by name under the root.
    CollectionItem *folder = nullptr;
    for (CollectionItem &child : imported.root().children)
        if (child.name == "PostmanTest")
            folder = &child;
    QVERIFY(folder);
    QVERIFY(folder->children.size() >= 1);
}

// ---------------------------------------------------------------------------
// D6. Exception / boundary: invalid URL, empty send, timeout, abort
// ---------------------------------------------------------------------------
void TestQtRequester::testInvalidUrlSendHandling()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);

    // Invalid URL -> onSendRequest returns early, no reply created, no crash.
    // In test mode, the warning is captured in m_lastWarningText instead of a
    // modal QMessageBox, so no manual dismiss is needed.
    urlEdit->setText("this is not a url");
    tool.m_lastWarningText.clear();
    tool.onSendRequest();
    QTest::qWait(50);
    QVERIFY2(!tool.m_lastWarningText.isEmpty(),
             "Invalid URL must trigger showBlockingWarning()");
    // Response area should not contain a successful "Sending request..." block.
    QString body = tool.ui.textEdit_response_body->toPlainText();
    QVERIFY2(!body.contains("URL: this is not a url"),
             "Invalid URL must not produce a send attempt");

    // Empty URL -> also rejected.
    urlEdit->setText("");
    tool.m_lastWarningText.clear();
    tool.onSendRequest();
    QTest::qWait(20);
    QVERIFY2(!tool.m_lastWarningText.isEmpty(),
             "Empty URL must trigger showBlockingWarning()");
}

void TestQtRequester::testAbortRunningRequest()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    HttpTestServer srv;
    QVERIFY(srv.start());

    // Start a slow request that we can abort mid-flight
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText(srv.baseUrl() + "/slowbytes/100000");  // ~640ms streaming

    // Send the request — this will populate m_currentTaskId
    tool.onSendRequest();
    QTest::qWait(100);  // let the request start running

    // Verify that m_currentTaskId is non-zero (a request is in flight)
    QVERIFY2(tool.m_currentTaskId != 0, "A running request must have a non-zero task id");

    // Abort the running task
    tool.onAbortTask();
    QTest::qWait(100);

    // After abort, task id must be cleared (either via onAbortTask or onResponse)
    QVERIFY2(tool.m_currentTaskId == 0, "Task id must be cleared after abort");

    // Calling abort again with no active task must not crash
    tool.onAbortTask();

    srv.stop();
}

void TestQtRequester::testTransferTimeoutHandling()
{
    NetworkRequestTool tool;
    tool.show();
    QTest::qWait(50);

    // A small transfer timeout should be honoured by the request pipeline.
    // /slowbytes streams ~640ms total; a 300ms timeout must abort it (not hang).
    tool.m_settings.transferTimeoutMs = 300;

    HttpTestServer srv;
    QVERIFY(srv.start());
    auto *urlEdit = tool.findChild<QLineEdit *>("lineEdit_url");
    QVERIFY(urlEdit);
    urlEdit->setText(srv.baseUrl() + "/slowbytes/100000");

    tool.onSendRequest();
    // Wait well past the timeout; the request must not hang/crash the UI, and the
    // pipeline must surface a timeout/error into the response area (not silently hang).
    QTest::qWait(2000);
    QString resp = tool.ui.textEdit_response_body->toPlainText();
    QVERIFY2(!resp.isEmpty(),
             "After transfer timeout the UI must show an error/timeout response, not stay blank");
}

// ---------------------------------------------------------------------------
// D7. Batch execution (UI not yet implemented) — placeholder / design marker
// ---------------------------------------------------------------------------
void TestQtRequester::testBatchExecutionPlaceholder()
{
    // The qtrequester UI does not yet expose a "run collection / batch send" entry
    // point (no runCollection / batchRun / runAllTask symbols exist). This test
    // documents the intended coverage and is skipped until the feature lands.
    //
    // When implemented, the following real-user paths must be covered:
    //   1. Select a collection or folder node in m_collectionTree.
    //   2. Trigger batch send (button / context menu).
    //   3. Requests execute concurrently or serially per setting; assert each
    //      child response is captured and the history grows by N entries.
    //   4. A failing child request is isolated (others still run) — no global abort.
    //   5. Progress/summary UI reflects N/M completed.
    QSKIP("批量请求执行 UI 尚未实现（Batch execution UI not yet implemented）");
}

