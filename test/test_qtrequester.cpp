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
#include "test_qtrequester.h"
#include "networkrequesttool.h"
#include "requestcontext.h"
#include "responseresult.h"
#include "networkrequestmanager.h"
#include "networkreply.h"

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

    // None should not add header
    tool.ui.table_headers->setRowCount(0);
    tool.m_settings.authType = "None";
    tool.applyAuthHeader();
    headers = tool.getHeaders();
    QVERIFY(!headers.contains("Authorization"));
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
                         directOk = rsp && rsp->success && rsp->body.contains("GET");
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

