#include <QDebug>
#include <QStandardPaths>
#include "jsonsyntaxhighlighter.h"
#include "xmlsyntaxhighlighter.h"
#include <QPainter>
#include <QUrlQuery>
#include <QUuid>
#include "networkrequesttool.h"
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyledItemDelegate>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtGui/QTextCursor>
#include <QtGui/QTextCharFormat>
#include "networkrequestmanager.h"
#include "networkreply.h"

using namespace QtNetworkRequest;

namespace
{
    // Delegate for table cells. The window-level stylesheet styles QLineEdit with
    // generous padding (8px) intended for the standalone inputs; applied to the
    // short in-cell editor it clips the text vertically. Setting the stylesheet
    // directly on the editor widget guarantees precedence over the window sheet
    // (a widget's own stylesheet wins), so the text stays fully visible.
    class CompactCellDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override
        {
            QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
            if (editor)
                editor->setStyleSheet(QStringLiteral(
                    "QLineEdit {"
                    " padding: 1px 4px; margin: 0px;"
                    " min-height: 0px;"
                    " border: 1px solid #0078d4;"
                    " border-radius: 0px; }"));
            return editor;
        }

        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const override
        {
            QStyledItemDelegate::updateEditorGeometry(editor, option, index);
            // Force the editor to fill the whole cell rect so it is not shrunk to
            // a shorter size-hint and bottom-aligned inside the row.
            if (editor)
                editor->setGeometry(option.rect);
        }
    };
}
NetworkRequestTool::NetworkRequestTool(QWidget *parent)
    : QMainWindow(parent), currentMethod("GET"), currentBodyType("none"), currentRawType("Text"), isNewRequest(true)
{
    ui.setupUi(this);
    initialize();

    // Connect table cell changed signal to listen for Content-Type header changes
    connect(ui.table_headers, &QTableWidget::cellChanged, this, [=](int row, int column)
            {
        QTableWidgetItem *keyItem = ui.table_headers->item(row, 0);
        QTableWidgetItem *valueItem = ui.table_headers->item(row, 1);
        if (keyItem && valueItem && keyItem->text().toLower() == "content-type") {
            // Update Body type when Content-Type header changes
            updateBodyTypeFromContentType(valueItem->text());
        } });
}

NetworkRequestTool::~NetworkRequestTool()
{
    unInitialize();
}

void NetworkRequestTool::initialize()
{
    NetworkRequestManager::initialize();

    initializeUI();
    initializeConnections();
    setupDefaultValues();
}

void NetworkRequestTool::unInitialize()
{
    NetworkRequestManager::unInitialize();
}

void NetworkRequestTool::initializeUI()
{
    // Set initial splitter ratio
    ui.splitter->setStretchFactor(0, 1); // History list
    ui.splitter->setStretchFactor(1, 3); // Request area

    // Set table column widths
    ui.table_params->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.table_headers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Set request and response area splitter ratio
    ui.splitter_request->setStretchFactor(0, 1); // Request area
    ui.splitter_request->setStretchFactor(1, 1); // Response area

    // Response info bar
    m_labelResponseInfo = new QLabel("Status: -- | Time: -- | Received: -- | Sent: --");
    m_labelResponseInfo->setStyleSheet("color: #969696; padding: 4px 8px; background: #252526;");
    auto *respPage = ui.tabWidget_response->widget(0);
    if (respPage)
    {
        auto *respLayout = qobject_cast<QVBoxLayout*>(respPage->layout());
        if (respLayout)
            respLayout->insertWidget(0, m_labelResponseInfo);
    }

    ui.table_body->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.stackedWidget_body->setCurrentWidget(ui.page_raw);

    // Set table to support row selection
    ui.table_params->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.table_headers->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.table_body->setSelectionBehavior(QAbstractItemView::SelectRows);

    // The window-level stylesheet gives QLineEdit/QComboBox generous padding and
    // a min-width meant for the standalone inputs. Inside a table cell that clips
    // the double-click edit text and makes the form-data type combo overflow the
    // short row. Give the tables a comfortable row height, a compact style for
    // cell widgets (e.g. the form-data type combo), and a delegate that styles
    // the transient in-cell editor on the editor itself (see CompactCellDelegate)
    // so the window sheet cannot override it and clip the text.
    const QString cellWidgetStyle = QStringLiteral(
        "QComboBox, QComboBox:focus, QComboBox:hover, QComboBox:focus:hover {"
        " padding: 1px 4px; margin: 0px;"
        " min-width: 0px; min-height: 0px;"
        " border-width: 1px; border-radius: 0px; }");
    for (QTableWidget *table : {ui.table_params, ui.table_headers, ui.table_body})
    {
        table->setStyleSheet(cellWidgetStyle);
        table->verticalHeader()->setDefaultSectionSize(32);
        table->setItemDelegate(new CompactCellDelegate(table));
        // Selecting a (full) row otherwise highlights the column header sections,
        // making the header bar look selected/blue (most obvious with a single
        // row). Disable section highlighting so only the row is selected.
        table->horizontalHeader()->setHighlightSections(false);
        table->verticalHeader()->setHighlightSections(false);
    }
}

void NetworkRequestTool::initializeConnections()
{
    // Request related
    connect(ui.cmb_method, &QComboBox::currentTextChanged, this, &NetworkRequestTool::onMethodChanged);
    connect(ui.cmb_body_type, &QComboBox::currentTextChanged, this, &NetworkRequestTool::onBodyTypeChanged);
    connect(ui.cmb_raw_type, &QComboBox::currentTextChanged, this, &NetworkRequestTool::onRawTypeChanged);
    connect(ui.btn_send, &QPushButton::clicked, this, &NetworkRequestTool::onSendRequest);
    connect(ui.btn_save, &QPushButton::clicked, this, &NetworkRequestTool::onSaveRequest);
    connect(ui.btn_settings, &QPushButton::clicked, this, &NetworkRequestTool::onSettingsClicked);
    connect(ui.btn_new_request, &QPushButton::clicked, this, &NetworkRequestTool::onNewRequest);

    // Parameters and request headers
    connect(ui.btn_add_param, &QPushButton::clicked, this, &NetworkRequestTool::onAddParam);
    connect(ui.btn_remove_param, &QPushButton::clicked, this, &NetworkRequestTool::onRemoveParam);
    connect(ui.btn_add_header, &QPushButton::clicked, this, &NetworkRequestTool::onAddHeader);
    connect(ui.btn_remove_header, &QPushButton::clicked, this, &NetworkRequestTool::onRemoveHeader);

    // History
    connect(ui.listWidget_history, &QListWidget::itemClicked, this, &NetworkRequestTool::onHistoryItemClicked);
    connect(ui.lineEdit_search, &QLineEdit::textChanged, this, &NetworkRequestTool::onSearchHistory);

    // Form data
    connect(ui.btn_add_body, &QPushButton::clicked, this, &NetworkRequestTool::onAddBodyParam);
    connect(ui.btn_remove_body, &QPushButton::clicked, this, &NetworkRequestTool::onRemoveBodyParam);
    connect(ui.cmb_body_type, &QComboBox::currentTextChanged, this, &NetworkRequestTool::onBodyTypeComboChanged);
    connect(ui.table_body, &QTableWidget::cellChanged, this, &NetworkRequestTool::onBodyParamTypeChanged);

    // Connect table cell changed signal to listen for Content-Type header changes
    connect(ui.table_headers, &QTableWidget::cellChanged, this, [=](int row, int column)
            {
        QTableWidgetItem *keyItem = ui.table_headers->item(row, 0);
        QTableWidgetItem *valueItem = ui.table_headers->item(row, 1);
        if (keyItem && valueItem && keyItem->text().toLower() == "content-type") {
            // Update Body type when Content-Type header changes
            updateBodyTypeFromContentType(valueItem->text());
        } });
}

void NetworkRequestTool::setupDefaultValues()
{
    // Set default HTTP method
    ui.cmb_method->setCurrentText("GET");

    // Add some common request headers
    addDefaultHeaders();
}

void NetworkRequestTool::addDefaultHeaders()
{
    QStringList defaultHeaders = {
        "Accept;*/*",
        "Connection;keep-alive",
        "User-Agent;QtNetworkTool/1.0"};

    // Get existing headers to avoid duplicates
    QSet<QString> existingHeaders;
    for (int i = 0; i < ui.table_headers->rowCount(); ++i)
    {
        QTableWidgetItem *keyItem = ui.table_headers->item(i, 0);
        if (keyItem)
        {
            existingHeaders.insert(keyItem->text().toLower());
        }
    }

    for (const QString &header : defaultHeaders)
    {
        QStringList parts = header.split(";");
        if (parts.size() >= 2)
        {
            QString headerName = parts[0];
            QString headerValue = parts[1];

            // Add the header only if it doesn't already exist
            if (!existingHeaders.contains(headerName.toLower()))
            {
                int row = ui.table_headers->rowCount();
                ui.table_headers->insertRow(row);
                ui.table_headers->setItem(row, 0, new QTableWidgetItem(headerName));
                ui.table_headers->setItem(row, 1, new QTableWidgetItem(headerValue));
            }
        }
    }
}

void NetworkRequestTool::onMethodChanged(const QString &method)
{
    currentMethod = method;

    bool enableBody = (method != "GET" && method != "HEAD" && method != "OPTIONS");
    ui.cmb_body_type->setEnabled(enableBody);
    ui.textEdit_body->setEnabled(enableBody && currentBodyType != "none");

    // Automatically set body type based on HTTP method
    if (!enableBody)
    {
        // For GET and HEAD methods, set body type to "none"
        ui.cmb_body_type->setCurrentText("none");
    }
    else
    {
        // For methods that require a body, if current is "none", set to default "raw"
        if (ui.cmb_body_type->currentText() == "none")
        {
            ui.cmb_body_type->setCurrentText("raw");
        }
    }

    // Update Content-Type header
    updateContentTypeHeader();

    // Only update default headers if this is a new request or if no headers exist
    if (isNewRequest || ui.table_headers->rowCount() == 0)
    {
        updateDefaultHeadersForMethod(method);
    }

    // Ensure the focus of the header table is normal
    ui.table_headers->setFocus();
}

void NetworkRequestTool::updateDefaultHeadersForMethod(const QString &method)
{
    // Get existing headers to avoid duplicates
    QSet<QString> existingHeaders;
    for (int i = 0; i < ui.table_headers->rowCount(); ++i)
    {
        QTableWidgetItem *keyItem = ui.table_headers->item(i, 0);
        if (keyItem && !keyItem->text().isEmpty())
        {
            existingHeaders.insert(keyItem->text().toLower());
        }
    }

    // Define default headers for different HTTP methods.
    // NOTE: "Accept-Encoding" is intentionally omitted so Qt can add it itself
    // and transparently decompress gzip/deflate responses.
    QMap<QString, QString> defaultHeaders;
    if (method == "GET" || method == "HEAD" || method == "OPTIONS")
    {
        defaultHeaders["Accept"] = "*/*";
        defaultHeaders["User-Agent"] = "QtNetworkTool/1.0";
    }
    else
    {
        defaultHeaders["Accept"] = "*/*";
        defaultHeaders["User-Agent"] = "QtNetworkTool/1.0";
    }

    // Add default headers only if they don't already exist
    for (auto it = defaultHeaders.begin(); it != defaultHeaders.end(); ++it)
    {
        if (!existingHeaders.contains(it.key().toLower()))
        {
            int row = ui.table_headers->rowCount();
            ui.table_headers->insertRow(row);
            ui.table_headers->setItem(row, 0, new QTableWidgetItem(it.key()));
            ui.table_headers->setItem(row, 1, new QTableWidgetItem(it.value()));
        }
    }
}

// New helper function
bool NetworkRequestTool::isDefaultHeader(const QString &strHeader)
{
    // Define default header list
    static const QStringList defaultHeaders = {
        QLatin1String("Accept"),
        QLatin1String("Accept-Encoding"),
        QLatin1String("User-Agent"),
        QLatin1String("Content-Type"),
        QLatin1String("Connection")};

    // Check if it's a default header
    return defaultHeaders.contains(strHeader, Qt::CaseInsensitive);
}

void NetworkRequestTool::onBodyTypeChanged(const QString &bodyType)
{
    currentBodyType = bodyType;

    if (bodyType == "none")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_raw);
        ui.textEdit_body->clear();
        ui.textEdit_body->setEnabled(false);
    }
    else if (bodyType == "raw")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_raw);
        ui.textEdit_body->setEnabled(true);
        // Ensure JSON auto-formatting function is correctly connected in initial state
        if (currentRawType == "JSON")
        {
            connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
        }
    }
    else
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_form);
        ui.textEdit_body->setEnabled(false);
    }

    // Update Content-Type header
    updateContentTypeHeader();

    // Set appropriate syntax highlighting
    if (bodyType == "raw")
    {
        if (currentRawType == "JSON")
        {
            // TODO: Set JSON syntax highlighting
            // Connect textChanged signal to implement auto-formatting
            connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
        }
        else if (currentRawType == "XML")
        {
            // TODO: Set XML syntax highlighting
        }
        else
        {
            // Disconnect to avoid triggering in non-JSON mode
            disconnect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged);
        }
    }
}

void NetworkRequestTool::onRawTypeChanged(const QString &type)
{
    currentRawType = type;
    updateContentTypeHeader();

    // Set appropriate syntax highlighting
    if (type == "JSON")
    {
        // TODO: Set JSON syntax highlighting
        // Connect textChanged signal to implement auto-formatting
        connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
    }
    else
    {
        // Disconnect to avoid triggering in non-JSON mode
        disconnect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged);
        if (type == "XML")
        {
            // TODO: Set XML syntax highlighting
        }
    }
}

void NetworkRequestTool::updateHeader(const QString &key, const QString &value)
{
    // Remove existing headers with the same key (case-insensitive)
    for (int i = ui.table_headers->rowCount() - 1; i >= 0; --i)
    {
        QTableWidgetItem *keyItem = ui.table_headers->item(i, 0);
        if (keyItem && keyItem->text().compare(key, Qt::CaseInsensitive) == 0)
        {
            ui.table_headers->removeRow(i);
        }
    }

    // Add the new header
    int row = ui.table_headers->rowCount();
    ui.table_headers->insertRow(row);
    ui.table_headers->setItem(row, 0, new QTableWidgetItem(key));
    ui.table_headers->setItem(row, 1, new QTableWidgetItem(value));
}

void NetworkRequestTool::updateContentTypeHeader()
{
    QString contentType;
    if (currentBodyType == "none")
    {
        // none type does not need Content-Type
        return;
    }
    else if (currentBodyType == "raw")
    {
        if (currentRawType == "JSON")
        {
            contentType = "application/json";
        }
        else if (currentRawType == "XML")
        {
            contentType = "application/xml";
        }
        else if (currentRawType == "HTML")
        {
            contentType = "text/html";
        }
        else
        {
            contentType = "text/plain";
        }
    }
    else if (currentBodyType == "form-data")
    {
        // Keep existing boundary if present, otherwise generate new one
        if (currentBoundary.isEmpty())
        {
            // Generate unique boundary
            QString uuid = QUuid::createUuid().toString();
            // Remove braces
            uuid = uuid.mid(1, uuid.length() - 2);
            currentBoundary = uuid;
        }
        contentType = QString("multipart/form-data; boundary=%1").arg(currentBoundary);
    }
    else if (currentBodyType == "x-www-form-urlencoded")
    {
        contentType = "application/x-www-form-urlencoded";
    }

    // Update or add Content-Type header
    updateHeader("Content-Type", contentType);
}

void NetworkRequestTool::onSendRequest()
{
    QString url = buildUrlWithParams();
    if (url.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a valid URL");
        return;
    }

    applyAuthHeader();

    std::unique_ptr<RequestContext> req = std::make_unique<RequestContext>();
    req->url = url;
    req->type = getRequestType();
    req->headers = getHeaders();
    req->body = getRequestBody();
    req->behavior.maxRedirectionCount = 3;
    applyRequestSettings(req);
    if (req->type == RequestType::Download || req->type == RequestType::MTDownload)
    {
        // 示例代码，此工具无这两种type
        req->downloadConfig = std::make_unique<DownloadConfig>();
        req->downloadConfig->saveFileName = "";
        req->downloadConfig->saveDir = "";
        req->downloadConfig->overwriteFile = true;
        req->downloadConfig->threadCount = 32;
        req->behavior.showProgress = true;
    }
    else if (req->type == RequestType::Upload)
    {
        // 示例代码，此工具无这这种type
        req->uploadConfig = std::make_unique<UploadConfig>();
        req->uploadConfig->filePath = "your file path";
        req->uploadConfig->usePutMethod = true;
        req->behavior.showProgress = true;
    }
    if (req->type == RequestType::Post && currentBodyType == "form-data")
    {
        req->uploadConfig = std::make_unique<UploadConfig>();
        req->uploadConfig->useFormData = true;
		req->uploadConfig->files = files;
		req->uploadConfig->kvPairs = kvPairs;
    }

    // Create network request
    std::shared_ptr<NetworkReply> pReply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    if (pReply)
    {
        connect(pReply.get(), &NetworkReply::requestFinished,
                this, &NetworkRequestTool::onResponse);

        clearResponse();
        appendToResponseBody("Sending request...\n", QColor(0, 120, 212));
        appendToResponseBody("URL: " + url + "\n", QColor(204, 204, 204));
        appendToResponseBody("Method: " + currentMethod + "\n\n", QColor(204, 204, 204));
    }
}

RequestType NetworkRequestTool::getRequestType()
{
    if (currentMethod == "GET")     return RequestType::Get;
    if (currentMethod == "POST")    return RequestType::Post;
    if (currentMethod == "PUT")     return RequestType::Put;
    if (currentMethod == "PATCH")   return RequestType::Patch;
    if (currentMethod == "DELETE")  return RequestType::Delete;
    if (currentMethod == "HEAD")    return RequestType::Head;
    if (currentMethod == "OPTIONS") return RequestType::Options;
    return RequestType::Get;
}

QString NetworkRequestTool::buildUrlWithParams()
{
    QString baseUrl = ui.lineEdit_url->text().trimmed();
    if (baseUrl.isEmpty())
    {
        return QString();
    }

    QUrl url(baseUrl);
    if (!url.isValid())
    {
        return QString();
    }

    // Add query parameters
    QUrlQuery query;
    int rows = ui.table_params->rowCount();
    for (int i = 0; i < rows; i++)
    {
        QTableWidgetItem *keyItem = ui.table_params->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_params->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
        {
            query.addQueryItem(keyItem->text(), valueItem->text());
        }
    }

    if (!query.isEmpty())
    {
        url.setQuery(query);
    }

    return url.toString();
}

QMap<QByteArray, QByteArray> NetworkRequestTool::getHeaders()
{
    QMap<QByteArray, QByteArray> headers;
    int rows = ui.table_headers->rowCount();
    for (int i = 0; i < rows; i++)
    {
        QTableWidgetItem *keyItem = ui.table_headers->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_headers->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
        {
            headers[keyItem->text().toUtf8()] = valueItem->text().toUtf8();
        }
    }
    return headers;
}

QString NetworkRequestTool::getRequestBody()
{
    if (currentBodyType == "none" || currentMethod == "GET" || currentMethod == "HEAD" || currentMethod == "OPTIONS")
    {
        return QString();
    }

    if (currentBodyType == "raw")
    {
        return ui.textEdit_body->toPlainText();
    }
    else if (currentBodyType == "form-data")
    {
        QString body;
        files.clear();
        kvPairs.clear();
        // Iterate through form data table
        for (int i = 0; i < ui.table_body->rowCount(); ++i)
        {
            QTableWidgetItem *keyItem = ui.table_body->item(i, 0);
            QTableWidgetItem *valueItem = ui.table_body->item(i, 1);
            QTableWidgetItem *typeItem = ui.table_body->item(i, 2);
            QWidget *pWidget = ui.table_body->cellWidget(i, 2);
            QComboBox *box = qobject_cast<QComboBox *>(pWidget);

            if (keyItem && valueItem && !keyItem->text().isEmpty())
            {
                if (box && box->currentText().toLower() == "file")
                {
                    // Handle file upload
                    QFileInfo fileInfo(valueItem->text());
                    if (fileInfo.exists())
                    {
                        files.append(valueItem->text());
                    }
                }
                else
                {
                    // Handle plain text
                    kvPairs.insert(keyItem->text(), valueItem->text());
                }
            }
        }
        return body;
    }
    else if (currentBodyType == "x-www-form-urlencoded")
    {
        QUrlQuery query;
        // Iterate through form data table
        for (int i = 0; i < ui.table_body->rowCount(); ++i)
        {
            QTableWidgetItem *keyItem = ui.table_body->item(i, 0);
            QTableWidgetItem *valueItem = ui.table_body->item(i, 1);

            if (keyItem && valueItem && !keyItem->text().isEmpty())
            {
                query.addQueryItem(keyItem->text(), valueItem->text());
            }
        }
        return query.toString();
    }

    return QString();
}

void NetworkRequestTool::onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
{
    // Safety check to prevent crashes during object destruction
    if (!ui.textEdit_response_body || !ui.textEdit_response_headers)
    {
        return;
    }
    clearResponse();
    if (rsp->isSuccess())
    {
        displayResponseHeaders(rsp->headers);

        m_highlighter.reset();
        if (isJsonResponse(rsp->headers))
        {
            m_highlighter = std::make_unique<JsonSyntaxHighlighter>(ui.textEdit_response_body->document());
            displayJsonResponse(rsp->body);
        }
        else if (isXmlResponse(rsp->headers))
        {
            m_highlighter = std::make_unique<XmlSyntaxHighlighter>(ui.textEdit_response_body->document());
            appendToResponseBody(rsp->body, QColor(16, 124, 16));
        }
        else
        {
            appendToResponseBody(rsp->body, QColor(16, 124, 16));
        }
    }
    else
    {
        appendToResponseBody("Error: \n" + rsp->error.message, QColor(232, 17, 35));
    }

    // Item 2: show status code, time, size
    QString info = QString("Status: %1 | Time: %2 ms | Received: %3 | Sent: %4")
                       .arg(rsp->statusCode)
                       .arg(rsp->performance.durationMs)
                       .arg(bytesToString(rsp->performance.bytesReceived))
                       .arg(bytesToString(rsp->performance.bytesSent));
    m_labelResponseInfo->setText(info);
}

bool NetworkRequestTool::isJsonResponse(const QMap<QByteArray, QByteArray> &headers)
{
    QByteArray contentType = headers.value("Content-Type").toLower();
    return contentType.contains("application/json");
}

bool NetworkRequestTool::isXmlResponse(const QMap<QByteArray, QByteArray> &headers)
{
    QByteArray contentType = headers.value("Content-Type").toLower();
    return contentType.contains("xml");
}

bool NetworkRequestTool::isOctetStreamResponse(const QMap<QByteArray, QByteArray> &headers)
{
    QByteArray contentType = headers.value("Content-Type").toLower();
    return contentType.contains("application/octet-stream");
}

void NetworkRequestTool::displayJsonResponse(const QString &response)
{
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (!doc.isNull())
    {
        appendToResponse(doc.toJson(QJsonDocument::Indented), QColor(16, 124, 16));
    }
    else
    {
        appendToResponse(response, QColor(16, 124, 16));
    }
}

void NetworkRequestTool::appendToResponse(const QString &text, const QColor &color)
{
    appendToResponseBody(text, color);
}

void NetworkRequestTool::appendToResponseBody(const QString &text, const QColor &color)
{
    if (!ui.textEdit_response_body)
    {
        return;
    }

    QTextCharFormat format;
    format.setForeground(color);

    QTextCursor cursor = ui.textEdit_response_body->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
}

void NetworkRequestTool::appendToResponseHeaders(const QString &text, const QColor &color)
{
    if (!ui.textEdit_response_headers)
    {
        return;
    }

    QTextCharFormat format;
    format.setForeground(color);

    QTextCursor cursor = ui.textEdit_response_headers->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
}

void NetworkRequestTool::clearResponse()
{
    clearResponseBody();
    clearResponseHeaders();
}

void NetworkRequestTool::clearResponseBody()
{
    if (ui.textEdit_response_body)
    {
        ui.textEdit_response_body->clear();
    }
}

void NetworkRequestTool::clearResponseHeaders()
{
    if (ui.textEdit_response_headers)
    {
        ui.textEdit_response_headers->clear();
    }
}

void NetworkRequestTool::displayResponseHeaders(const QMap<QByteArray, QByteArray> &headers)
{
    // Safety check to prevent crashes during object destruction
    if (!ui.textEdit_response_headers)
    {
        return;
    }

    clearResponseHeaders();

    appendToResponseHeaders("Response Headers:\n", QColor(0, 120, 212));
    appendToResponseHeaders("================\n", QColor(0, 120, 212));

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
    {
        QString headerLine = QString("%1: %2\n").arg(QString::fromUtf8(it.key())).arg(QString::fromUtf8(it.value()));
        appendToResponseHeaders(headerLine, QColor(204, 204, 204));
    }
}

void NetworkRequestTool::onAddParam()
{
    int row = ui.table_params->rowCount();
    ui.table_params->insertRow(row);
}

void NetworkRequestTool::onRemoveParam()
{
    // Prioritize deleting currently selected rows
    QList<QTableWidgetSelectionRange> ranges = ui.table_params->selectedRanges();
    if (!ranges.isEmpty())
    {
        for (int i = ranges.size() - 1; i >= 0; --i)
        {
            int top = ranges[i].topRow();
            int bottom = ranges[i].bottomRow();
            for (int row = bottom; row >= top; --row)
            {
                ui.table_params->removeRow(row);
            }
        }
    }
}

void NetworkRequestTool::onAddHeader()
{
    int row = ui.table_headers->rowCount();
    ui.table_headers->insertRow(row);
}

void NetworkRequestTool::onRemoveHeader()
{
    QList<QTableWidgetSelectionRange> ranges = ui.table_headers->selectedRanges();
    if (!ranges.isEmpty())
    {
        for (int i = ranges.size() - 1; i >= 0; --i)
        {
            int top = ranges[i].topRow();
            int bottom = ranges[i].bottomRow();
            for (int row = bottom; row >= top; --row)
            {
                ui.table_headers->removeRow(row);
            }
        }
    }
}

void NetworkRequestTool::onAbortTask()
{
}

void NetworkRequestTool::onAbortAllTask()
{
    NetworkRequestManager::globalInstance()->stopAllRequest();
}

QString NetworkRequestTool::bytesToString(qint64 bytes)
{
    QString str;
    if (bytes < 1024)
    {
        str = QString("%1B").arg(bytes);
    }
    else if (bytes < 1024 * 1024)
    {
        bytes = bytes / 1024;
        str = QString("%1KB").arg(bytes);
    }
    else if (bytes < 1024 * 1024 * 1024)
    {
        qreal dSize = (qreal)bytes / 1024 / 1024;
        char ch[8] = {0};
        sprintf(ch, "%.2f", dSize);
        str = QString("%1MB").arg(ch);
    }
    else
    {
        qreal dSize = (qreal)bytes / 1024 / 1024 / 1024;
        char ch[8] = {0};
        sprintf(ch, "%.2f", dSize);
        str = QString("%1GB").arg(ch);
    }
    return str;
}

QString NetworkRequestTool::getDefaultDownloadDir()
{
    const QStringList &lstDir = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
    if (!lstDir.isEmpty())
    {
        return lstDir[0];
    }
    return QLatin1String("download/");
}

void NetworkRequestTool::onNewRequest()
{
    // Clear URL and request body
    ui.lineEdit_url->clear();
    ui.textEdit_body->clear();

    // Reset HTTP method
    ui.cmb_method->setCurrentText("GET");
    currentMethod = "GET";

    // Reset request body type
    ui.cmb_body_type->setCurrentText("none");
    currentBodyType = "none";
    ui.cmb_raw_type->setCurrentText("Text");
    currentRawType = "Text";
    ui.cmb_raw_type->setEnabled(false);
    ui.textEdit_body->setEnabled(false);

    // Clear parameter table
    ui.table_params->setRowCount(0);

    // Clear header table and add default headers
    ui.table_headers->setRowCount(0);
    addDefaultHeaders();

    clearResponse();

    isNewRequest = true;
    if (m_labelResponseInfo)
        m_labelResponseInfo->setText("Status: -- | Time: -- | Received: -- | Sent: --");
}

void NetworkRequestTool::onSaveRequest()
{
    if (ui.lineEdit_url->text().isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a URL before saving");
        return;
    }
    saveToHistory();

    ensureStorageDir();
    QString fileName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".json";
    QString filePath = storageDir() + "/" + fileName;
    saveToDisk(filePath);
}

void NetworkRequestTool::saveToHistory()
{
    RequestHistory history;
    history.method = currentMethod;
    history.url = ui.lineEdit_url->text();
    history.body = ui.textEdit_body->toPlainText();
    history.bodyType = currentBodyType;
    history.rawType = currentRawType;
    history.timestamp = QDateTime::currentDateTime();

    // Save parameters
    for (int i = 0; i < ui.table_params->rowCount(); ++i)
    {
        QTableWidgetItem *keyItem = ui.table_params->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_params->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
        {
            history.params[keyItem->text()] = valueItem->text();
        }
    }

    // Save request headers
    for (int i = 0; i < ui.table_headers->rowCount(); ++i)
    {
        QTableWidgetItem *keyItem = ui.table_headers->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_headers->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
        {
            history.headers[keyItem->text()] = valueItem->text();
        }
    }

    requestHistory.prepend(history);
    if (requestHistory.size() > 100)
    { // Limit history size
        requestHistory.removeLast();
    }

    updateHistoryList();
}

void NetworkRequestTool::loadFromHistory(const RequestHistory &history)
{
    // Load basic information
    ui.cmb_method->setCurrentText(history.method);
    ui.lineEdit_url->setText(history.url);
    ui.cmb_body_type->setCurrentText(history.bodyType);
    ui.cmb_raw_type->setCurrentText(history.rawType);
    ui.textEdit_body->setText(history.body);

    // Load parameters
    ui.table_params->setRowCount(0);
    for (auto it = history.params.constBegin(); it != history.params.constEnd(); ++it)
    {
        int row = ui.table_params->rowCount();
        ui.table_params->insertRow(row);
        ui.table_params->setItem(row, 0, new QTableWidgetItem(it.key()));
        ui.table_params->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    // Load request headers
    ui.table_headers->setRowCount(0);
    for (auto it = history.headers.constBegin(); it != history.headers.constEnd(); ++it)
    {
        int row = ui.table_headers->rowCount();
        ui.table_headers->insertRow(row);
        ui.table_headers->setItem(row, 0, new QTableWidgetItem(it.key()));
        ui.table_headers->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    // Clear response
    clearResponse();
}

void NetworkRequestTool::updateHistoryList()
{
    ui.listWidget_history->clear();
    for (const RequestHistory &history : requestHistory)
    {
        QString displayText = QString("[%1] %2 %3")
                                  .arg(formatDateTime(history.timestamp))
                                  .arg(history.method)
                                  .arg(history.url);
        QListWidgetItem *item = new QListWidgetItem(displayText);
        item->setToolTip(history.url);
        ui.listWidget_history->addItem(item);
    }
}

void NetworkRequestTool::onHistoryItemClicked(QListWidgetItem *item)
{
    int index = ui.listWidget_history->row(item);
    if (index >= 0 && index < requestHistory.size())
    {
        loadFromHistory(requestHistory[index]);
        isNewRequest = false;
    }
}

void NetworkRequestTool::onSearchHistory(const QString &text)
{
    for (int i = 0; i < ui.listWidget_history->count(); ++i)
    {
        QListWidgetItem *item = ui.listWidget_history->item(i);
        bool matches = item->text().contains(text, Qt::CaseInsensitive) ||
                       item->toolTip().contains(text, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }
}

void NetworkRequestTool::clearRequestForm()
{
    // Clear URL and request body
    if (ui.lineEdit_url)
        ui.lineEdit_url->clear();
    if (ui.textEdit_body)
        ui.textEdit_body->clear();
    if (ui.textEdit_response_body)
        ui.textEdit_response_body->clear();
    if (ui.textEdit_response_headers)
        ui.textEdit_response_headers->clear();

    // Reset HTTP method
    if (ui.cmb_method)
        ui.cmb_method->setCurrentText("GET");

    // Reset request body type
    if (ui.cmb_body_type)
        ui.cmb_body_type->setCurrentText("none");
    if (ui.cmb_raw_type)
    {
        ui.cmb_raw_type->setCurrentText("Text");
        ui.cmb_raw_type->setEnabled(false);
    }
    if (ui.textEdit_body)
        ui.textEdit_body->setEnabled(false);

    // Clear parameters table
    if (ui.table_params)
        ui.table_params->setRowCount(0);

    // Clear request headers table and add default headers
    if (ui.table_headers)
    {
        ui.table_headers->setRowCount(0);
        addDefaultHeaders();
    }

    // Reset current state
    currentMethod = "GET";
    currentBodyType = "none";
    currentRawType = "Text";
}

QString NetworkRequestTool::formatDateTime(const QDateTime &dateTime)
{
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

void NetworkRequestTool::onBodyTypeComboChanged(const QString &type)
{
    if (type == "raw")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_raw);
        ui.cmb_raw_type->setEnabled(true);
    }
    else if (type == "none")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_raw);
        ui.cmb_raw_type->setEnabled(false);
        ui.textEdit_body->clear();
    }
    else
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_form);
        ui.cmb_raw_type->setEnabled(false);
    }
}

void NetworkRequestTool::onAddBodyParam()
{
    int row = ui.table_body->rowCount();
    ui.table_body->insertRow(row);

    // Add type selection dropdown
    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItem("Text");
    typeCombo->addItem("File");
    connect(typeCombo, &QComboBox::currentTextChanged, this, [=](const QString &)
            {
            // Emit the cellChanged signal manually when combo box changes
            emit ui.table_body->cellChanged(row, 2); });

    ui.table_body->setCellWidget(row, 2, typeCombo);
}

void NetworkRequestTool::onRemoveBodyParam()
{
    QList<QTableWidgetSelectionRange> ranges = ui.table_body->selectedRanges();
    if (!ranges.isEmpty())
    {
        for (int i = ranges.size() - 1; i >= 0; --i)
        {
            int top = ranges[i].topRow();
            int bottom = ranges[i].bottomRow();
            for (int row = bottom; row >= top; --row)
            {
                ui.table_body->removeRow(row);
            }
        }
    }
}

void NetworkRequestTool::onBodyParamTypeChanged(int row, int column)
{
    if (column == 2) // Type column
    {
        QComboBox *typeCombo = qobject_cast<QComboBox *>(ui.table_body->cellWidget(row, column));
        if (typeCombo && typeCombo->currentText() == "File")
        {
            // Open file selection dialog
            QString fileName = QFileDialog::getOpenFileName(this, "Select File");
            if (!fileName.isEmpty())
            {
                QTableWidgetItem *valueItem = ui.table_body->item(row, 1);
                if (!valueItem)
                {
                    valueItem = new QTableWidgetItem();
                    ui.table_body->setItem(row, 1, valueItem);
                }
                valueItem->setText(fileName);
            }
        }
    }
}

void NetworkRequestTool::updateBodyTypeFromContentType(const QString &contentType)
{
    if (contentType.startsWith("application/json"))
    {
        ui.cmb_body_type->setCurrentText("raw");
        ui.cmb_raw_type->setCurrentText("JSON");
    }
    else if (contentType.startsWith("application/xml") || contentType.startsWith("text/xml"))
    {
        ui.cmb_body_type->setCurrentText("raw");
        ui.cmb_raw_type->setCurrentText("XML");
    }
    else if (contentType.startsWith("text/html"))
    {
        ui.cmb_body_type->setCurrentText("raw");
        ui.cmb_raw_type->setCurrentText("HTML");
    }
    else if (contentType.startsWith("text/plain"))
    {
        ui.cmb_body_type->setCurrentText("raw");
        ui.cmb_raw_type->setCurrentText("Text");
    }
    else if (contentType.startsWith("application/x-www-form-urlencoded"))
    {
        ui.cmb_body_type->setCurrentText("x-www-form-urlencoded");
    }
    else if (contentType.startsWith("multipart/form-data"))
    {
        ui.cmb_body_type->setCurrentText("form-data");
        // Parse boundary (if present)
        int boundaryIndex = contentType.indexOf("boundary=");
        if (boundaryIndex != -1)
        {
            QString boundary = contentType.mid(boundaryIndex + 9); // 9 is the length of "boundary="
            // Remove possible quotes
            if (boundary.startsWith("\"") && boundary.endsWith("\""))
            {
                boundary = boundary.mid(1, boundary.length() - 2);
            }
            currentBoundary = boundary;
        }
    }
}

void NetworkRequestTool::onBodyTextChanged()
{
    // Only auto-format in JSON mode
    if (currentRawType == "JSON")
    {
        QTextCursor cursor = ui.textEdit_body->textCursor();
        int cursorPosition = cursor.position();
        QString text = ui.textEdit_body->toPlainText();
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && !doc.isNull())
        {
            QString formattedJson = doc.toJson(QJsonDocument::Indented);
            disconnect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged);
            ui.textEdit_body->setPlainText(formattedJson);
            cursor.setPosition(qMin(cursorPosition, formattedJson.length()));
            ui.textEdit_body->setTextCursor(cursor);
            connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
        }
    }
}

// --- Settings dialog (items 3, 5) ---
void NetworkRequestTool::onSettingsClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Request Settings");
    dlg.setMinimumWidth(440);
    // The dialog inherits the main-window stylesheet which covers QDialog,
    // QGroupBox, QCheckBox, QSpinBox, QPushButton, and QDialogButtonBox with
    // the VSCode-inspired dark theme, ensuring visual consistency.

    auto *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 14, 16, 14);

    // ---------- Authorization ----------
    auto *authGroup = new QGroupBox("Authorization");
    auto *authLayout = new QFormLayout(authGroup);
    authLayout->setSpacing(5);
    authLayout->setContentsMargins(12, 10, 12, 10);
    authLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *authTypeCombo = new QComboBox();
    authTypeCombo->addItems({"None", "Basic", "Bearer"});
    authTypeCombo->setCurrentText(m_settings.authType);
    auto *authUserEdit = new QLineEdit(m_settings.authUsername);
    authUserEdit->setPlaceholderText("Username");
    auto *authPassEdit = new QLineEdit(m_settings.authPassword);
    authPassEdit->setPlaceholderText("Password");
    authPassEdit->setEchoMode(QLineEdit::Password);
    auto *authTokenEdit = new QLineEdit(m_settings.authToken);
    authTokenEdit->setPlaceholderText("Token");

    authLayout->addRow("Type:", authTypeCombo);
    authLayout->addRow("User:", authUserEdit);
    authLayout->addRow("Password:", authPassEdit);
    authLayout->addRow("Token:", authTokenEdit);

    auto onAuthTypeChanged = [=](const QString &type) {
        bool isBasic = (type == "Basic");
        bool isBearer = (type == "Bearer");
        authUserEdit->setVisible(isBasic);
        authPassEdit->setVisible(isBasic);
        authTokenEdit->setVisible(isBearer);
        // Hide the label row when the field is hidden so the layout collapses
        authLayout->labelForField(authUserEdit)->setVisible(isBasic);
        authLayout->labelForField(authPassEdit)->setVisible(isBasic);
        authLayout->labelForField(authTokenEdit)->setVisible(isBearer);
    };
    connect(authTypeCombo, &QComboBox::currentTextChanged, onAuthTypeChanged);
    onAuthTypeChanged(authTypeCombo->currentText());

    mainLayout->addWidget(authGroup);

    // ---------- Proxy ----------
    auto *proxyGroup = new QGroupBox("Proxy");
    auto *proxyLayout = new QFormLayout(proxyGroup);
    proxyLayout->setSpacing(5);
    proxyLayout->setContentsMargins(12, 10, 12, 10);
    proxyLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *proxyCheck = new QCheckBox("Enable proxy");
    proxyCheck->setChecked(m_settings.proxyEnabled);
    auto *proxyHostEdit = new QLineEdit(m_settings.proxyHost);
    proxyHostEdit->setPlaceholderText("host");
    auto *proxyPortSpin = new QSpinBox();
    proxyPortSpin->setRange(1, 65535);
    proxyPortSpin->setValue(m_settings.proxyPort);
    auto *proxyUserEdit = new QLineEdit(m_settings.proxyUser);
    proxyUserEdit->setPlaceholderText("User (optional)");
    auto *proxyPassEdit = new QLineEdit(m_settings.proxyPass);
    proxyPassEdit->setPlaceholderText("Password (optional)");
    proxyPassEdit->setEchoMode(QLineEdit::Password);

    proxyLayout->addRow(proxyCheck);
    proxyLayout->addRow("Host:", proxyHostEdit);
    proxyLayout->addRow("Port:", proxyPortSpin);
    proxyLayout->addRow("User:", proxyUserEdit);
    proxyLayout->addRow("Password:", proxyPassEdit);

    auto onProxyToggled = [=](bool checked) {
        proxyHostEdit->setEnabled(checked);
        proxyPortSpin->setEnabled(checked);
        proxyUserEdit->setEnabled(checked);
        proxyPassEdit->setEnabled(checked);
    };
    connect(proxyCheck, &QCheckBox::toggled, onProxyToggled);
    onProxyToggled(m_settings.proxyEnabled);

    mainLayout->addWidget(proxyGroup);

    // ---------- Timeout & Retry ----------
    auto *trGroup = new QGroupBox("Timeout & Retry");
    auto *trLayout = new QFormLayout(trGroup);
    trLayout->setSpacing(5);
    trLayout->setContentsMargins(12, 10, 12, 10);
    trLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto *timeoutSpin = new QSpinBox();
    timeoutSpin->setRange(1000, 300000);
    timeoutSpin->setSingleStep(1000);
    timeoutSpin->setValue(m_settings.transferTimeoutMs);
    timeoutSpin->setSuffix(" ms");
    auto *retryCheck = new QCheckBox("Enable retry on failure");
    retryCheck->setChecked(m_settings.retryEnabled);
    auto *retryCountSpin = new QSpinBox();
    retryCountSpin->setRange(1, 10);
    retryCountSpin->setValue(m_settings.maxRetryCount);
    auto *retryDelaySpin = new QSpinBox();
    retryDelaySpin->setRange(100, 30000);
    retryDelaySpin->setSingleStep(100);
    retryDelaySpin->setValue(m_settings.retryDelayMs);
    retryDelaySpin->setSuffix(" ms");

    trLayout->addRow("Timeout:", timeoutSpin);
    trLayout->addRow(retryCheck);
    trLayout->addRow("Max retries:", retryCountSpin);
    trLayout->addRow("Base delay:", retryDelaySpin);

    auto onRetryToggled = [=](bool checked) {
        retryCountSpin->setEnabled(checked);
        retryDelaySpin->setEnabled(checked);
    };
    connect(retryCheck, &QCheckBox::toggled, onRetryToggled);
    onRetryToggled(m_settings.retryEnabled);

    mainLayout->addWidget(trGroup);

    // -- Buttons --
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted)
    {
        m_settings.authType = authTypeCombo->currentText();
        m_settings.authUsername = authUserEdit->text();
        m_settings.authPassword = authPassEdit->text();
        m_settings.authToken = authTokenEdit->text();
        m_settings.proxyEnabled = proxyCheck->isChecked();
        m_settings.proxyHost = proxyHostEdit->text();
        m_settings.proxyPort = static_cast<quint16>(proxyPortSpin->value());
        m_settings.proxyUser = proxyUserEdit->text();
        m_settings.proxyPass = proxyPassEdit->text();
        m_settings.transferTimeoutMs = timeoutSpin->value();
        m_settings.retryEnabled = retryCheck->isChecked();
        m_settings.maxRetryCount = retryCountSpin->value();
        m_settings.retryDelayMs = retryDelaySpin->value();
    }
}

void NetworkRequestTool::applyAuthHeader()
{
    if (m_settings.authType == "Basic" && !m_settings.authUsername.isEmpty())
    {
        QString creds = m_settings.authUsername + ":" + m_settings.authPassword;
        QString encoded = creds.toUtf8().toBase64();
        updateHeader("Authorization", "Basic " + encoded);
    }
    else if (m_settings.authType == "Bearer" && !m_settings.authToken.isEmpty())
    {
        updateHeader("Authorization", "Bearer " + m_settings.authToken);
    }
}

void NetworkRequestTool::applyRequestSettings(std::unique_ptr<RequestContext> &req)
{
    req->behavior.transferTimeout = m_settings.transferTimeoutMs;
    req->behavior.retryOnFailed = m_settings.retryEnabled;
    req->behavior.maxRetryCount = m_settings.maxRetryCount;
    req->behavior.retryDelayMs = m_settings.retryDelayMs;

    if (m_settings.proxyEnabled && !m_settings.proxyHost.isEmpty())
    {
        req->proxyConfig = std::make_unique<ProxyConfig>();
        req->proxyConfig->enabled = true;
        req->proxyConfig->host = m_settings.proxyHost;
        req->proxyConfig->port = m_settings.proxyPort;
        req->proxyConfig->user = m_settings.proxyUser;
        req->proxyConfig->password = m_settings.proxyPass;
    }
}

// --- Persistent storage (item 4) ---
QString NetworkRequestTool::storageDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/requests";
}

void NetworkRequestTool::ensureStorageDir()
{
    QDir dir(storageDir());
    if (!dir.exists())
        dir.mkpath(".");
}

void NetworkRequestTool::saveToDisk(const QString &filePath)
{
    QJsonObject obj;
    obj["method"] = currentMethod;
    obj["url"] = ui.lineEdit_url->text();
    obj["body"] = ui.textEdit_body->toPlainText();
    obj["bodyType"] = currentBodyType;
    obj["rawType"] = currentRawType;
    obj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray paramsArr;
    for (int i = 0; i < ui.table_params->rowCount(); ++i)
    {
        auto *k = ui.table_params->item(i, 0);
        auto *v = ui.table_params->item(i, 1);
        if (k && v && !k->text().isEmpty())
        {
            QJsonObject p;
            p["key"] = k->text();
            p["value"] = v->text();
            paramsArr.append(p);
        }
    }
    obj["params"] = paramsArr;

    QJsonArray headersArr;
    for (int i = 0; i < ui.table_headers->rowCount(); ++i)
    {
        auto *k = ui.table_headers->item(i, 0);
        auto *v = ui.table_headers->item(i, 1);
        if (k && v && !k->text().isEmpty())
        {
            QJsonObject h;
            h["key"] = k->text();
            h["value"] = v->text();
            headersArr.append(h);
        }
    }
    obj["headers"] = headersArr;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void NetworkRequestTool::loadFromDisk(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();

    ui.cmb_method->setCurrentText(obj["method"].toString());
    ui.lineEdit_url->setText(obj["url"].toString());
    ui.cmb_body_type->setCurrentText(obj["bodyType"].toString("none"));
    ui.cmb_raw_type->setCurrentText(obj["rawType"].toString("Text"));
    ui.textEdit_body->setText(obj["body"].toString());

    ui.table_params->setRowCount(0);
    for (const auto &p : obj["params"].toArray())
    {
        QJsonObject po = p.toObject();
        int row = ui.table_params->rowCount();
        ui.table_params->insertRow(row);
        ui.table_params->setItem(row, 0, new QTableWidgetItem(po["key"].toString()));
        ui.table_params->setItem(row, 1, new QTableWidgetItem(po["value"].toString()));
    }

    ui.table_headers->setRowCount(0);
    for (const auto &h : obj["headers"].toArray())
    {
        QJsonObject ho = h.toObject();
        int row = ui.table_headers->rowCount();
        ui.table_headers->insertRow(row);
        ui.table_headers->setItem(row, 0, new QTableWidgetItem(ho["key"].toString()));
        ui.table_headers->setItem(row, 1, new QTableWidgetItem(ho["value"].toString()));
    }
    clearResponse();
}
