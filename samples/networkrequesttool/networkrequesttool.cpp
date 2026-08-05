#include <QDebug>
#include <QStandardPaths>
#include "jsonsyntaxhighlighter.h"
#include "xmlsyntaxhighlighter.h"
#include <QPainter>
#include <QUrlQuery>
#include <QUuid>
#include "networkrequesttool.h"
#include "postmanconverter.h"
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QCheckBox>
#include <QClipboard>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeWidget>
#include <QStyle>
#include <QApplication>
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
    loadEnvironments();
    loadCollection();
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

    // --- Environment selector (injected into request toolbar) ---
    auto *toolbarLayout = qobject_cast<QHBoxLayout *>(
        ui.frame_request_toolbar->layout());
    if (toolbarLayout)
    {
        // Spacer between Settings and the env section
        toolbarLayout->addStretch();

        m_cmbEnvironment = new QComboBox();
        m_cmbEnvironment->setObjectName("cmb_environment");
        m_cmbEnvironment->setMinimumWidth(120);
        m_cmbEnvironment->setToolTip("Active environment");
        toolbarLayout->addWidget(m_cmbEnvironment);

        m_btnManageEnv = new QPushButton("Env");
        m_btnManageEnv->setObjectName("btn_manage_env");
        m_btnManageEnv->setToolTip("Manage environments");
        m_btnManageEnv->setFixedWidth(40);
        toolbarLayout->addWidget(m_btnManageEnv);
    }

    // --- Response toolbar (injected into response body tab) ---
    buildResponseToolbar();

    // --- Collection panel (M4) ---
    buildCollectionPanel();
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

    // Environment
    if (m_cmbEnvironment)
        connect(m_cmbEnvironment, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &NetworkRequestTool::onEnvironmentChanged);
    if (m_btnManageEnv)
        connect(m_btnManageEnv, &QPushButton::clicked, this, &NetworkRequestTool::onManageEnvironments);

    // Collection tree
    if (m_collectionTree)
        connect(m_collectionTree, &QTreeWidget::itemClicked,
                this, &NetworkRequestTool::onCollectionItemClicked);

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
    connect(ui.btn_binary_browse, &QPushButton::clicked, this, [=]() {
        QString path = QFileDialog::getOpenFileName(this, "Select binary file");
        if (!path.isEmpty())
        {
            m_binaryFilePath = path;
            ui.lineEdit_binary_path->setText(path);
        }
    });
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
        ui.cmb_raw_type->setEnabled(false);
    }
    else if (bodyType == "raw")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_raw);
        ui.textEdit_body->setEnabled(true);
        ui.cmb_raw_type->setEnabled(true);
        // Ensure JSON auto-formatting function is correctly connected in initial state
        if (currentRawType == "JSON")
        {
            connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
        }
    }
    else if (bodyType == "binary")
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_binary);
        ui.textEdit_body->setEnabled(false);
        ui.cmb_raw_type->setEnabled(false);
    }
    else
    {
        ui.stackedWidget_body->setCurrentWidget(ui.page_form);
        ui.textEdit_body->setEnabled(false);
        ui.cmb_raw_type->setEnabled(false);
    }

    // Update Content-Type header
    updateContentTypeHeader();

    // Set appropriate syntax highlighting
    if (bodyType == "raw")
    {
        if (currentRawType == "JSON")
        {
            // Connect textChanged signal to implement auto-formatting
            connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
        }
        else
        {
            // Disconnect to avoid triggering in non-JSON mode
            disconnect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged);
        }
        applyBodySyntaxHighlighting(currentRawType);
    }
    else
    {
        // Non-raw body types (binary / form-data) use no syntax highlighting
        m_highlighter.reset();
    }
}

void NetworkRequestTool::onRawTypeChanged(const QString &type)
{
    currentRawType = type;
    updateContentTypeHeader();

    // Set appropriate syntax highlighting
    if (type == "JSON")
    {
        // Connect textChanged signal to implement auto-formatting
        connect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged, Qt::UniqueConnection);
    }
    else
    {
        // Disconnect to avoid triggering in non-JSON mode
        disconnect(ui.textEdit_body, &QTextEdit::textChanged, this, &NetworkRequestTool::onBodyTextChanged);
    }
    applyBodySyntaxHighlighting(type);
}

void NetworkRequestTool::applyBodySyntaxHighlighting(const QString &rawType)
{
    // Recreate the highlighter bound to the request body document. Reassigning
    // a new unique_ptr replaces (and thereby clears) any previous highlighter.
    if (rawType == "JSON")
    {
        m_highlighter = std::make_unique<JsonSyntaxHighlighter>(ui.textEdit_body->document());
    }
    else if (rawType == "XML")
    {
        m_highlighter = std::make_unique<XmlSyntaxHighlighter>(ui.textEdit_body->document());
    }
    else
    {
        m_highlighter.reset();
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
    QString raw = ui.lineEdit_url->text().trimmed();
    if (raw.isEmpty() || !QUrl(raw).isValid())
    {
        QMessageBox::warning(this, "Error", "Please enter a valid URL");
        return;
    }

    applyAuthHeader();   // preview header write-back (Approach A)

    std::unique_ptr<RequestContext> req = buildRequestContext();

    // Create network request
    std::shared_ptr<NetworkReply> pReply = NetworkRequestManager::globalInstance()->postRequest(std::move(req));
    if (pReply)
    {
        connect(pReply.get(), &NetworkReply::requestFinished,
                this, &NetworkRequestTool::onResponse);

        clearResponse();
        appendToResponseBody("Sending request...\n", QColor(0, 120, 212));
        appendToResponseBody("URL: " + raw + "\n", QColor(204, 204, 204));
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
        auto params = getFormUrlEncodedMap();
        for (auto it = params.cbegin(); it != params.cend(); ++it)
            query.addQueryItem(it.key(), it.value());
        return query.toString();
    }

    return QString();
}

QString NetworkRequestTool::baseUrlFromInput() const
{
    QString raw = ui.lineEdit_url->text().trimmed();
    QUrl url(raw);
    if (!url.isValid())
        return raw;
    return url.adjusted(QUrl::RemoveQuery).toString();
}

QMap<QString, QString> NetworkRequestTool::getQueryParams() const
{
    QMap<QString, QString> params;
    int rows = ui.table_params->rowCount();
    for (int i = 0; i < rows; ++i)
    {
        QTableWidgetItem *keyItem = ui.table_params->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_params->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
            params.insert(keyItem->text(), valueItem->text());
    }
    return params;
}

QMap<QString, QString> NetworkRequestTool::getFormUrlEncodedMap() const
{
    QMap<QString, QString> params;
    int rows = ui.table_body->rowCount();
    for (int i = 0; i < rows; ++i)
    {
        QTableWidgetItem *keyItem = ui.table_body->item(i, 0);
        QTableWidgetItem *valueItem = ui.table_body->item(i, 1);
        if (keyItem && valueItem && !keyItem->text().isEmpty())
            params.insert(keyItem->text(), valueItem->text());
    }
    return params;
}

QByteArray NetworkRequestTool::readBinaryFile(const QString &path) const
{
    if (path.isEmpty())
        return QByteArray();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

std::unique_ptr<RequestContext> NetworkRequestTool::buildRequestContext()
{
    RequestContextBuilder builder;
    builder.url(baseUrlFromInput())
        .type(getRequestType())
        .headers(getHeaders())
        .queryParams(getQueryParams())
        .environment(m_envStore.activeVariables())
        .authConfig(buildAuthConfig());

    // Body type -> builder method mapping
    if (currentBodyType == "raw")
    {
        const QString text = ui.textEdit_body->toPlainText();
        if (currentRawType == "JSON")
            builder.bodyJson(text);
        else if (currentRawType == "XML")
            builder.bodyXml(text);
        else
            builder.bodyRaw(text);
    }
    else if (currentBodyType == "x-www-form-urlencoded")
    {
        builder.bodyFormUrlEncoded(getFormUrlEncodedMap());
    }
    else if (currentBodyType == "binary")
    {
        builder.bodyBinary(readBinaryFile(m_binaryFilePath));
    }
    // form-data is handled via UploadConfig below; "none" sets no body.

    auto req = builder.build();
    applyRequestSettings(req);
    if (req->type == RequestType::Post && currentBodyType == "form-data")
    {
        req->uploadConfig = std::make_unique<UploadConfig>();
        req->uploadConfig->useFormData = true;
        req->uploadConfig->files = files;
        req->uploadConfig->kvPairs = kvPairs;
    }
    return req;
}

void NetworkRequestTool::onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp)
{
    // Safety check to prevent crashes during object destruction
    if (!ui.textEdit_response_body || !ui.textEdit_response_headers)
    {
        return;
    }
    clearResponse();
    m_lastResponseBody.clear();
    m_isResponseJson = false;
    m_searchSelections.clear();
    m_currentSearchHit = -1;
    if (rsp->isSuccess())
    {
        displayResponseHeaders(rsp->headers);

        m_highlighter.reset();
        QByteArray contentType = rsp->contentType().toLower();
        if (contentType.contains("application/json"))
        {
            m_highlighter = std::make_unique<JsonSyntaxHighlighter>(ui.textEdit_response_body->document());
            QJsonDocument doc = rsp->json();
            m_lastResponseBody = rsp->body;
            m_isResponseJson = !doc.isNull();
            QString pretty = doc.isNull() ? rsp->body
                                          : QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            displayJsonResponse(pretty);
        }
        else if (contentType.contains("xml"))
        {
            m_highlighter = std::make_unique<XmlSyntaxHighlighter>(ui.textEdit_response_body->document());
            m_lastResponseBody = rsp->body;
            appendToResponseBody(rsp->body, QColor(16, 124, 16));
        }
        else
        {
            m_lastResponseBody = rsp->body;
            appendToResponseBody(rsp->body, QColor(16, 124, 16));
        }

        displayResponseCookies(rsp->cookies);
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

void NetworkRequestTool::displayResponseCookies(const QList<QNetworkCookie> &cookies)
{
    if (!ui.textEdit_response_cookies)
        return;
    if (cookies.isEmpty())
    {
        ui.textEdit_response_cookies->setPlainText("(no cookies)");
        return;
    }
    QStringList lines;
    for (const QNetworkCookie &c : cookies)
    {
        QString line = QString("%1 = %2").arg(QString::fromUtf8(c.name()), QString::fromUtf8(c.value()));
        if (!c.domain().isEmpty())
            line += QString("; Domain=%1").arg(c.domain());
        if (!c.path().isEmpty())
            line += QString("; Path=%1").arg(c.path());
        if (!c.expirationDate().isNull())
            line += QString("; Expires=%1").arg(c.expirationDate().toString(Qt::ISODate));
        lines.append(line);
    }
    ui.textEdit_response_cookies->setPlainText(lines.join("\n"));
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
    clearResponseCookies();
}

void NetworkRequestTool::clearResponseCookies()
{
    if (ui.textEdit_response_cookies)
    {
        ui.textEdit_response_cookies->clear();
    }
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
    authTypeCombo->addItems({"None", "Basic", "Bearer", "ApiKey", "OAuth2"});
    authTypeCombo->setCurrentText(m_settings.authType);
    auto *authUserEdit = new QLineEdit(m_settings.authUsername);
    authUserEdit->setPlaceholderText("Username");
    auto *authPassEdit = new QLineEdit(m_settings.authPassword);
    authPassEdit->setPlaceholderText("Password");
    authPassEdit->setEchoMode(QLineEdit::Password);
    auto *authTokenEdit = new QLineEdit(m_settings.authToken);
    authTokenEdit->setPlaceholderText("Token");
    auto *authApiKeyEdit = new QLineEdit(m_settings.authApiKey);
    authApiKeyEdit->setPlaceholderText("Header / Query key");
    auto *authApiValueEdit = new QLineEdit(m_settings.authApiValue);
    authApiValueEdit->setPlaceholderText("API key value");
    auto *authApiLocCombo = new QComboBox();
    authApiLocCombo->addItems({"Header", "Query"});
    authApiLocCombo->setCurrentText(m_settings.authApiLocation.isEmpty() ? "Header" : m_settings.authApiLocation);

    authLayout->addRow("Type:", authTypeCombo);
    authLayout->addRow("User:", authUserEdit);
    authLayout->addRow("Password:", authPassEdit);
    authLayout->addRow("Token:", authTokenEdit);
    authLayout->addRow("API Key:", authApiKeyEdit);
    authLayout->addRow("API Value:", authApiValueEdit);
    authLayout->addRow("API Location:", authApiLocCombo);

    // OAuth2 fields
    auto *oauthGrantCombo = new QComboBox();
    oauthGrantCombo->addItems({"Client Credentials", "Password", "Refresh Token"});
    if (!m_settings.oauthGrantType.isEmpty())
        oauthGrantCombo->setCurrentText(m_settings.oauthGrantType);
    auto *oauthClientIdEdit = new QLineEdit(m_settings.oauthClientId);
    oauthClientIdEdit->setPlaceholderText("Client ID");
    auto *oauthClientSecretEdit = new QLineEdit(m_settings.oauthClientSecret);
    oauthClientSecretEdit->setPlaceholderText("Client Secret");
    oauthClientSecretEdit->setEchoMode(QLineEdit::Password);
    auto *oauthScopesEdit = new QLineEdit(m_settings.oauthScopes);
    oauthScopesEdit->setPlaceholderText("scope1 scope2 (optional)");
    auto *oauthTokenUrlEdit = new QLineEdit(m_settings.oauthTokenUrl);
    oauthTokenUrlEdit->setPlaceholderText("https://auth.example.com/oauth/token");
    auto *oauthUserEdit = new QLineEdit(m_settings.oauthUsername);
    oauthUserEdit->setPlaceholderText("Username (Password grant)");
    auto *oauthPassEdit = new QLineEdit(m_settings.oauthPassword);
    oauthPassEdit->setPlaceholderText("Password (Password grant)");
    oauthPassEdit->setEchoMode(QLineEdit::Password);
    auto *oauthRefreshEdit = new QLineEdit(m_settings.oauthRefreshToken);
    oauthRefreshEdit->setPlaceholderText("Refresh token");

    authLayout->addRow("Grant:", oauthGrantCombo);
    authLayout->addRow("Client ID:", oauthClientIdEdit);
    authLayout->addRow("Client Secret:", oauthClientSecretEdit);
    authLayout->addRow("Scopes:", oauthScopesEdit);
    authLayout->addRow("Token URL:", oauthTokenUrlEdit);
    authLayout->addRow("User:", oauthUserEdit);
    authLayout->addRow("Password:", oauthPassEdit);
    authLayout->addRow("Refresh Token:", oauthRefreshEdit);

    auto onAuthTypeChanged = [=](const QString &type) {
        bool isBasic = (type == "Basic");
        bool isBearer = (type == "Bearer");
        bool isApiKey = (type == "ApiKey");
        bool isOAuth2 = (type == "OAuth2");
        authUserEdit->setVisible(isBasic);
        authPassEdit->setVisible(isBasic);
        authTokenEdit->setVisible(isBearer);
        authApiKeyEdit->setVisible(isApiKey);
        authApiValueEdit->setVisible(isApiKey);
        authApiLocCombo->setVisible(isApiKey);
        oauthGrantCombo->setVisible(isOAuth2);
        oauthClientIdEdit->setVisible(isOAuth2);
        oauthClientSecretEdit->setVisible(isOAuth2);
        oauthScopesEdit->setVisible(isOAuth2);
        oauthTokenUrlEdit->setVisible(isOAuth2);
        // Password grant fields: only show when Password is selected
        bool isOAuth2Password = isOAuth2 && oauthGrantCombo->currentText() == "Password";
        oauthUserEdit->setVisible(isOAuth2Password);
        oauthPassEdit->setVisible(isOAuth2Password);
        // Refresh Token field: only show when Refresh Token is selected
        bool isOAuth2Refresh = isOAuth2 && oauthGrantCombo->currentText() == "Refresh Token";
        oauthRefreshEdit->setVisible(isOAuth2Refresh);
        // Hide labels
        authLayout->labelForField(authUserEdit)->setVisible(isBasic);
        authLayout->labelForField(authPassEdit)->setVisible(isBasic);
        authLayout->labelForField(authTokenEdit)->setVisible(isBearer);
        authLayout->labelForField(authApiKeyEdit)->setVisible(isApiKey);
        authLayout->labelForField(authApiValueEdit)->setVisible(isApiKey);
        authLayout->labelForField(authApiLocCombo)->setVisible(isApiKey);
        authLayout->labelForField(oauthGrantCombo)->setVisible(isOAuth2);
        authLayout->labelForField(oauthClientIdEdit)->setVisible(isOAuth2);
        authLayout->labelForField(oauthClientSecretEdit)->setVisible(isOAuth2);
        authLayout->labelForField(oauthScopesEdit)->setVisible(isOAuth2);
        authLayout->labelForField(oauthTokenUrlEdit)->setVisible(isOAuth2);
        authLayout->labelForField(oauthUserEdit)->setVisible(isOAuth2Password);
        authLayout->labelForField(oauthPassEdit)->setVisible(isOAuth2Password);
        authLayout->labelForField(oauthRefreshEdit)->setVisible(isOAuth2Refresh);
    };
    connect(authTypeCombo, &QComboBox::currentTextChanged, onAuthTypeChanged);
    connect(oauthGrantCombo, &QComboBox::currentTextChanged, [=]() {
        onAuthTypeChanged(authTypeCombo->currentText());
    });
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
        m_settings.authApiKey = authApiKeyEdit->text();
        m_settings.authApiValue = authApiValueEdit->text();
        m_settings.authApiLocation = authApiLocCombo->currentText();
        m_settings.oauthGrantType = oauthGrantCombo->currentText();
        m_settings.oauthClientId = oauthClientIdEdit->text();
        m_settings.oauthClientSecret = oauthClientSecretEdit->text();
        m_settings.oauthScopes = oauthScopesEdit->text();
        m_settings.oauthTokenUrl = oauthTokenUrlEdit->text();
        m_settings.oauthUsername = oauthUserEdit->text();
        m_settings.oauthPassword = oauthPassEdit->text();
        m_settings.oauthRefreshToken = oauthRefreshEdit->text();
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
    else if (m_settings.authType == "ApiKey")
    {
        // Header placement: write a preview header (the library will skip it as
        // it is already present). Query placement: do NOT write to headers, the
        // library appends it to the URL query instead.
        if (m_settings.authApiLocation != "Query" && !m_settings.authApiKey.isEmpty())
            updateHeader(m_settings.authApiKey, m_settings.authApiValue);
    }
}

AuthConfig NetworkRequestTool::buildAuthConfig() const
{
    if (m_settings.authType == "Basic" && !m_settings.authUsername.isEmpty())
        return AuthConfig::basic(m_settings.authUsername, m_settings.authPassword);
    if (m_settings.authType == "Bearer" && !m_settings.authToken.isEmpty())
        return AuthConfig::bearer(m_settings.authToken);
    if (m_settings.authType == "ApiKey" && !m_settings.authApiKey.isEmpty())
    {
        ApiKeyPlacement loc = (m_settings.authApiLocation == "Query")
            ? ApiKeyPlacement::QueryParam
            : ApiKeyPlacement::Header;
        return AuthConfig::apiKeyAuth(m_settings.authApiKey, m_settings.authApiValue, loc);
    }
    if (m_settings.authType == "OAuth2" && !m_settings.oauthClientId.isEmpty())
    {
        AuthConfig::OAuth2Config oa;
        oa.clientId     = m_settings.oauthClientId;
        oa.clientSecret = m_settings.oauthClientSecret;
        oa.scopes       = m_settings.oauthScopes;
        oa.tokenUrl     = m_settings.oauthTokenUrl;

        if (m_settings.oauthGrantType == "Password")
        {
            oa.grant    = OAuth2GrantType::Password;
            oa.username = m_settings.oauthUsername;
            oa.password = m_settings.oauthPassword;
            // Seed refreshToken for potential 401 auto-renewal
            oa.refreshToken = m_settings.oauthRefreshToken;
        }
        else if (m_settings.oauthGrantType == "Refresh Token")
        {
            oa.grant        = OAuth2GrantType::RefreshToken;
            oa.refreshToken = m_settings.oauthRefreshToken;
        }
        else
        {
            oa.grant = OAuth2GrantType::ClientCredentials;
        }
        return AuthConfig::oauth2(oa);
    }
    return AuthConfig(); // None
}

void NetworkRequestTool::applyRequestSettings(std::unique_ptr<RequestContext> &req)
{
    req->behavior.maxRedirectionCount = 3;
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

// --- Environment management (M1) ---

void NetworkRequestTool::loadEnvironments()
{
    ensureStorageDir();
    const QString envPath = storageDir() + "/environments.json";
    if (!m_envStore.load(envPath))
    {
        // File doesn't exist yet — start with a default "dev" environment.
        QMap<QString, QString> vars;
        vars["host"] = "localhost";
        vars["port"] = "8080";
        m_envStore.upsert("Dev", vars);
        m_envStore.setActiveName("");
        m_envStore.save(envPath);
    }
    populateEnvironmentCombo();
}

void NetworkRequestTool::saveEnvironments()
{
    ensureStorageDir();
    m_envStore.save(storageDir() + "/environments.json");
}

void NetworkRequestTool::populateEnvironmentCombo()
{
    if (!m_cmbEnvironment)
        return;

    // Block signals during rebuild to avoid triggering onEnvironmentChanged
    m_cmbEnvironment->blockSignals(true);
    m_cmbEnvironment->clear();
    m_cmbEnvironment->addItem("No Environment", QString());

    const QStringList names = m_envStore.environmentNames();
    for (const QString &name : names)
        m_cmbEnvironment->addItem(name, name);

    // Restore selection
    const QString active = m_envStore.activeName();
    const int idx = active.isEmpty() ? 0 : m_cmbEnvironment->findData(active);
    m_cmbEnvironment->setCurrentIndex(idx >= 0 ? idx : 0);
    m_cmbEnvironment->blockSignals(false);
}

void NetworkRequestTool::onEnvironmentChanged(int /*index*/)
{
    if (!m_cmbEnvironment)
        return;

    const QString name = m_cmbEnvironment->currentData().toString();
    m_envStore.setActiveName(name);
}

void NetworkRequestTool::onManageEnvironments()
{
    // --- Build a simple Manage Environments dialog ---
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Environments");
    dlg.resize(500, 400);

    auto *mainLayout = new QVBoxLayout(&dlg);

    // Environment list
    auto *listWidget = new QListWidget(&dlg);
    const QStringList names = m_envStore.environmentNames();
    for (const QString &name : names)
        listWidget->addItem(name);
    mainLayout->addWidget(listWidget);

    // Add / Remove / Rename buttons
    auto *btnLayout = new QHBoxLayout();
    auto *btnAdd = new QPushButton("Add", &dlg);
    auto *btnRemove = new QPushButton("Remove", &dlg);
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnRemove);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // Variable editor table
    auto *varTable = new QTableWidget(0, 2, &dlg);
    varTable->setHorizontalHeaderLabels({"Variable", "Value"});
    varTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    mainLayout->addWidget(varTable);

    // Add/Remove row buttons for the variable table
    auto *varBtnLayout = new QHBoxLayout();
    auto *btnAddVar = new QPushButton("+ Variable", &dlg);
    auto *btnRemoveVar = new QPushButton("- Variable", &dlg);
    varBtnLayout->addWidget(btnAddVar);
    varBtnLayout->addWidget(btnRemoveVar);
    varBtnLayout->addStretch();
    mainLayout->addLayout(varBtnLayout);

    // Dialog buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    mainLayout->addWidget(buttonBox);

    // --- Sync: load variables for the currently selected environment ---
    auto loadVarsForEnv = [&](const QString &envName) {
        varTable->setRowCount(0);
        if (envName.isEmpty())
            return;
        for (const auto &entry : m_envStore.activeVariables())
            ; // not used — we need per-env access
        // Workaround: rebuild from scratch using the names list
        // Actually we need to iterate m_envStore per the selected name.
        // For now, re-populate from known data using a simple approach.
    };

    // Since EnvironmentStore doesn't expose per-env variables publicly,
    // we use upsert-then-read pattern. For now, implement a simpler approach
    // that stores a working copy.
    QMap<QString, QMap<QString, QString>> envVars;
    // Initialize working copies from the store by iterating names
    {
        // We need per-env variable access. Add a simple member to hold them.
        // Rebuild by selective load/save.
    }

    // --- Simpler approach: embed the working state in lambdas ---
    // Pre-populate all env data from the store
    // Since we can't iterate envs easily from the public API, rebuild from the
    // combination of names + known data.
    // Use a fresh store copy approach — load from file again.
    EnvironmentStore workStore;
    const QString envPath = storageDir() + "/environments.json";
    workStore.load(envPath);

    // Current selected env in the dialog
    QString selectedEnv;

    // Populate variable table when an environment is selected
    QObject::connect(listWidget, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *current, QListWidgetItem * /*prev*/) {
        if (!current)
            return;
        selectedEnv = current->text();
        varTable->setRowCount(0);
        // We need per-env access. Since we loaded workStore, re-derive:
        // The simplest workaround: track variables manually
    });

    // Since per-env variable access requires iterating environments,
    // and the public API doesn't expose it, let me add a helper or refactor.
    // For M1, use a pragmatic approach: manually map env name -> vars.
    QMap<QString, QMap<QString, QString>> varsMap;
    {
        // We need to get all envs. Load them from the file and map.
        QFile file(envPath);
        if (file.open(QIODevice::ReadOnly))
        {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonArray envs = doc.object()["environments"].toArray();
            for (const auto &ev : envs)
            {
                QJsonObject eo = ev.toObject();
                QString ename = eo["name"].toString();
                QMap<QString, QString> vmap;
                QJsonObject vobj = eo["variables"].toObject();
                for (auto it = vobj.begin(); it != vobj.end(); ++it)
                    vmap[it.key()] = it.value().toString();
                varsMap[ename] = vmap;
            }
        }
    }

    // Re-connect with proper vars access
    QObject::disconnect(listWidget, &QListWidget::currentItemChanged, nullptr, nullptr);
    QObject::connect(listWidget, &QListWidget::currentItemChanged,
                     [&](QListWidgetItem *current, QListWidgetItem * /*prev*/) {
        if (!current)
            return;
        selectedEnv = current->text();
        varTable->setRowCount(0);
        const auto &vars = varsMap[selectedEnv];
        for (auto it = vars.begin(); it != vars.end(); ++it)
        {
            int row = varTable->rowCount();
            varTable->insertRow(row);
            varTable->setItem(row, 0, new QTableWidgetItem(it.key()));
            varTable->setItem(row, 1, new QTableWidgetItem(it.value()));
        }
    });

    // Initial selection
    if (listWidget->count() > 0)
        listWidget->setCurrentRow(0);

    // Add environment
    QObject::connect(btnAdd, &QPushButton::clicked, [&]() {
        bool ok = false;
        QString name = QInputDialog::getText(&dlg, "Add Environment", "Name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty())
        {
            listWidget->addItem(name);
            varsMap[name] = {};
            listWidget->setCurrentRow(listWidget->count() - 1);
        }
    });

    // Remove environment
    QObject::connect(btnRemove, &QPushButton::clicked, [&]() {
        int row = listWidget->currentRow();
        if (row < 0)
            return;
        QString name = listWidget->currentItem()->text();
        varsMap.remove(name);
        delete listWidget->takeItem(row);
        if (listWidget->count() > 0)
            listWidget->setCurrentRow(0);
        varTable->setRowCount(0);
    });

    // Add variable row
    QObject::connect(btnAddVar, &QPushButton::clicked, [&]() {
        int row = varTable->rowCount();
        varTable->insertRow(row);
        varTable->setItem(row, 0, new QTableWidgetItem(""));
        varTable->setItem(row, 1, new QTableWidgetItem(""));
    });

    // Remove variable row
    QObject::connect(btnRemoveVar, &QPushButton::clicked, [&]() {
        int row = varTable->currentRow();
        if (row >= 0)
            varTable->removeRow(row);
    });

    // Dialog accepted → persist
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        // Save current table state to varsMap
        if (!selectedEnv.isEmpty())
        {
            QMap<QString, QString> vars;
            for (int i = 0; i < varTable->rowCount(); ++i)
            {
                QTableWidgetItem *keyItem = varTable->item(i, 0);
                QTableWidgetItem *valItem = varTable->item(i, 1);
                QString key = keyItem ? keyItem->text().trimmed() : QString();
                if (!key.isEmpty())
                    vars[key] = valItem ? valItem->text() : QString();
            }
            varsMap[selectedEnv] = vars;
        }
        // Rebuild the store
        m_envStore = EnvironmentStore();
        for (auto it = varsMap.begin(); it != varsMap.end(); ++it)
            m_envStore.upsert(it.key(), it.value());
        // Preserve active name if still valid
        const QString prevActive = m_envStore.activeName();
        if (prevActive.isEmpty() || !varsMap.contains(prevActive))
            m_envStore.setActiveName("");
        else
            m_envStore.setActiveName(prevActive);
        saveEnvironments();
        populateEnvironmentCombo();
        dlg.accept();
    });

    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    dlg.exec();
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

    // ── R2: serialize auth so Collections / Postman round-trips preserve auth ──
    QJsonObject authObj;
    authObj["type"] = m_settings.authType;
    if (m_settings.authType == "Basic")
    {
        authObj["username"] = m_settings.authUsername;
        authObj["password"] = m_settings.authPassword;
    }
    else if (m_settings.authType == "Bearer")
    {
        authObj["token"] = m_settings.authToken;
    }
    else if (m_settings.authType == "ApiKey")
    {
        authObj["key"]      = m_settings.authApiKey;
        authObj["value"]    = m_settings.authApiValue;
        authObj["location"] = m_settings.authApiLocation;
    }
    else if (m_settings.authType == "OAuth2")
    {
        authObj["grantType"]    = m_settings.oauthGrantType;
        authObj["clientId"]     = m_settings.oauthClientId;
        authObj["clientSecret"] = m_settings.oauthClientSecret;
        authObj["scopes"]       = m_settings.oauthScopes;
        authObj["tokenUrl"]     = m_settings.oauthTokenUrl;
        authObj["username"]     = m_settings.oauthUsername;
        authObj["password"]     = m_settings.oauthPassword;
        authObj["refreshToken"] = m_settings.oauthRefreshToken;
    }
    obj["auth"] = authObj;

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
    loadRequestFromJson(doc.object());
}

void NetworkRequestTool::loadRequestFromJson(const QJsonObject &obj)
{
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

    // ── R2: restore auth from JSON ──
    const QJsonObject authObj = obj["auth"].toObject();
    if (!authObj.isEmpty())
    {
        m_settings.authType = authObj["type"].toString();
        if (m_settings.authType == "Basic")
        {
            m_settings.authUsername = authObj["username"].toString();
            m_settings.authPassword = authObj["password"].toString();
        }
        else if (m_settings.authType == "Bearer")
        {
            m_settings.authToken = authObj["token"].toString();
        }
        else if (m_settings.authType == "ApiKey")
        {
            m_settings.authApiKey      = authObj["key"].toString();
            m_settings.authApiValue    = authObj["value"].toString();
            m_settings.authApiLocation = authObj["location"].toString();
        }
        else if (m_settings.authType == "OAuth2")
        {
            m_settings.oauthGrantType    = authObj["grantType"].toString();
            m_settings.oauthClientId     = authObj["clientId"].toString();
            m_settings.oauthClientSecret = authObj["clientSecret"].toString();
            m_settings.oauthScopes       = authObj["scopes"].toString();
            m_settings.oauthTokenUrl     = authObj["tokenUrl"].toString();
            m_settings.oauthUsername     = authObj["username"].toString();
            m_settings.oauthPassword     = authObj["password"].toString();
            m_settings.oauthRefreshToken = authObj["refreshToken"].toString();
        }
    }

    clearResponse();
}

// ─── Response toolbar (M3) ───────────────────────────────────────────────────

void NetworkRequestTool::buildResponseToolbar()
{
    // Inject a compact toolbar at the top of the response body tab layout,
    // mirroring how m_labelResponseInfo was injected.
    auto *respPage = ui.tabWidget_response->widget(0); // Body tab
    if (!respPage)
        return;
    auto *respLayout = qobject_cast<QVBoxLayout *>(respPage->layout());
    if (!respLayout)
        return;

    m_responseToolbar = new QWidget();
    auto *hLayout = new QHBoxLayout(m_responseToolbar);
    hLayout->setContentsMargins(4, 2, 4, 2);
    hLayout->setSpacing(4);

    m_leResponseSearch = new QLineEdit();
    m_leResponseSearch->setPlaceholderText("Search response...");
    m_leResponseSearch->setClearButtonEnabled(true);
    m_leResponseSearch->setMaximumWidth(200);
    hLayout->addWidget(m_leResponseSearch);

    auto *btnPrev = new QPushButton("<");
    btnPrev->setFixedWidth(24);
    btnPrev->setToolTip("Previous match");
    hLayout->addWidget(btnPrev);

    auto *btnNext = new QPushButton(">");
    btnNext->setFixedWidth(24);
    btnNext->setToolTip("Next match");
    hLayout->addWidget(btnNext);

    hLayout->addStretch();

    auto *btnCopy = new QPushButton("Copy");
    btnCopy->setToolTip("Copy response body to clipboard");
    hLayout->addWidget(btnCopy);

    auto *btnSave = new QPushButton("Save");
    btnSave->setToolTip("Save response body to file");
    hLayout->addWidget(btnSave);

    auto *chkPretty = new QCheckBox("Pretty");
    chkPretty->setChecked(true);
    chkPretty->setToolTip("Toggle between pretty-printed and raw JSON");
    hLayout->addWidget(chkPretty);

    // Insert after m_labelResponseInfo (index 0)
    respLayout->insertWidget(1, m_responseToolbar);

    // Connections
    connect(m_leResponseSearch, &QLineEdit::textChanged,
            this, &NetworkRequestTool::onResponseSearchChanged);
    connect(m_leResponseSearch, &QLineEdit::returnPressed,
            this, &NetworkRequestTool::onResponseFindNext);
    connect(btnPrev, &QPushButton::clicked, this, &NetworkRequestTool::onResponseFindPrev);
    connect(btnNext, &QPushButton::clicked, this, &NetworkRequestTool::onResponseFindNext);
    connect(btnCopy, &QPushButton::clicked, this, &NetworkRequestTool::onResponseCopy);
    connect(btnSave, &QPushButton::clicked, this, &NetworkRequestTool::onResponseSave);
    connect(chkPretty, &QCheckBox::toggled, this, &NetworkRequestTool::onResponsePrettyToggled);
}

void NetworkRequestTool::onResponseSearchChanged(const QString & /*text*/)
{
    doResponseSearch();
}

void NetworkRequestTool::doResponseSearch()
{
    const QString searchText = m_leResponseSearch ? m_leResponseSearch->text() : QString();
    m_searchSelections.clear();
    m_currentSearchHit = -1;

    if (searchText.isEmpty())
    {
        ui.textEdit_response_body->setExtraSelections({});
        return;
    }

    QTextDocument *doc = ui.textEdit_response_body->document();
    QTextCursor cursor(doc);
    const QColor hiliteNormal(255, 255, 0, 80);   // semi-transparent yellow
    const QColor hiliteCurrent(255, 165, 0, 120); // orange

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor);
        if (!cursor.isNull())
        {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(hiliteNormal);
            sel.cursor = cursor;
            m_searchSelections.append(sel);
        }
    }

    if (!m_searchSelections.isEmpty())
    {
        m_currentSearchHit = 0;
        m_searchSelections[0].format.setBackground(hiliteCurrent);
    }

    ui.textEdit_response_body->setExtraSelections(m_searchSelections);
}

void NetworkRequestTool::navigateSearchHit(int delta)
{
    if (m_searchSelections.isEmpty())
        return;

    const int count = m_searchSelections.size();

    const QColor hiliteNormal(255, 255, 0, 80);
    const QColor hiliteCurrent(255, 165, 0, 120);

    if (m_currentSearchHit >= 0 && m_currentSearchHit < count)
        m_searchSelections[m_currentSearchHit].format.setBackground(hiliteNormal);

    m_currentSearchHit = (m_currentSearchHit + delta + count) % count;

    m_searchSelections[m_currentSearchHit].format.setBackground(hiliteCurrent);
    ui.textEdit_response_body->setExtraSelections(m_searchSelections);

    QTextCursor cursor = m_searchSelections[m_currentSearchHit].cursor;
    ui.textEdit_response_body->setTextCursor(cursor);
    ui.textEdit_response_body->ensureCursorVisible();
}

void NetworkRequestTool::onResponseFindPrev()
{
    navigateSearchHit(-1);
}

void NetworkRequestTool::onResponseFindNext()
{
    navigateSearchHit(+1);
}

void NetworkRequestTool::onResponseCopy()
{
    QApplication::clipboard()->setText(ui.textEdit_response_body->toPlainText());
}

void NetworkRequestTool::onResponseSave()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Response Body",
                                                 getDefaultDownloadDir() + "/response.txt",
                                                 "Text files (*.txt);;All files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(ui.textEdit_response_body->toPlainText().toUtf8());
        file.close();
    }
}

void NetworkRequestTool::onResponsePrettyToggled(bool checked)
{
    if (!m_isResponseJson)
        return;

    if (checked)
    {
        QJsonDocument doc = QJsonDocument::fromJson(m_lastResponseBody.toUtf8());
        if (!doc.isNull())
        {
            QString pretty = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            ui.textEdit_response_body->setPlainText(pretty);
        }
    }
    else
    {
        ui.textEdit_response_body->setPlainText(m_lastResponseBody);
    }

    if (m_leResponseSearch && !m_leResponseSearch->text().isEmpty())
        doResponseSearch();
}

// ─── Collection panel (M4) ───────────────────────────────────────────────────

void NetworkRequestTool::buildCollectionPanel()
{
    // Find the left-side panel layout (contains lineEdit_search, btn_new_request, listWidget_history).
    auto *parentLayout = qobject_cast<QVBoxLayout *>(ui.listWidget_history->parentWidget()->layout());
    if (!parentLayout)
        return;

    // ── Toolbar ──
    auto *collToolbar = new QWidget();
    auto *tbLayout = new QHBoxLayout(collToolbar);
    tbLayout->setContentsMargins(0, 4, 0, 2);
    tbLayout->setSpacing(2);

    auto *btnNewColl = new QPushButton("+Coll");
    btnNewColl->setToolTip("New collection");
    btnNewColl->setFixedHeight(22);
    tbLayout->addWidget(btnNewColl);

    auto *btnAddFolder = new QPushButton("+Folder");
    btnAddFolder->setToolTip("Add folder");
    btnAddFolder->setFixedHeight(22);
    tbLayout->addWidget(btnAddFolder);

    auto *btnAddReq = new QPushButton("+Req");
    btnAddReq->setToolTip("Add current request to collection");
    btnAddReq->setFixedHeight(22);
    tbLayout->addWidget(btnAddReq);

    tbLayout->addStretch();

    auto *btnImport = new QPushButton("Import");
    btnImport->setToolTip("Import Postman v2.1 collection");
    btnImport->setFixedHeight(22);
    tbLayout->addWidget(btnImport);

    auto *btnExport = new QPushButton("Export");
    btnExport->setToolTip("Export to Postman v2.1");
    btnExport->setFixedHeight(22);
    tbLayout->addWidget(btnExport);

    // Insert toolbar before the tree
    int insertIdx = parentLayout->indexOf(ui.listWidget_history);
    if (insertIdx < 0)
        insertIdx = parentLayout->count();
    parentLayout->insertWidget(insertIdx, collToolbar);

    // ── Tree ──
    m_collectionTree = new QTreeWidget();
    m_collectionTree->setHeaderHidden(true);
    m_collectionTree->setRootIsDecorated(true);
    m_collectionTree->setMinimumHeight(100);
    parentLayout->insertWidget(insertIdx + 1, m_collectionTree);

    // Connections
    connect(btnNewColl, &QPushButton::clicked, this, &NetworkRequestTool::onNewCollection);
    connect(btnAddFolder, &QPushButton::clicked, this, &NetworkRequestTool::onAddCollectionFolder);
    connect(btnAddReq, &QPushButton::clicked, this, &NetworkRequestTool::onAddCollectionRequest);
    connect(btnImport, &QPushButton::clicked, this, &NetworkRequestTool::onImportPostman);
    connect(btnExport, &QPushButton::clicked, this, &NetworkRequestTool::onExportPostman);
}

void NetworkRequestTool::loadCollection()
{
    ensureStorageDir();
    m_collectionPath = storageDir() + "/collection.json";
    QFile file(m_collectionPath);
    if (!file.exists())
        return;

    // Use the library's Collection::load which expects {name, items[]}
    // We use a simpler JSON format; for now just load via Collection::load
    if (m_collection.load(m_collectionPath))
        populateCollectionTree();
}

void NetworkRequestTool::saveCollection()
{
    ensureStorageDir();
    m_collection.save(m_collectionPath);
}

static void addItemToTree(const CollectionItem &item, QTreeWidgetItem *parent)
{
    auto *treeItem = new QTreeWidgetItem(parent);
    treeItem->setText(0, item.name);
    treeItem->setData(0, Qt::UserRole, item.id);
    treeItem->setData(0, Qt::UserRole + 1, static_cast<int>(item.type));

    if (item.type == CollectionItemType::Folder)
    {
        treeItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        for (const auto &child : item.children)
            addItemToTree(child, treeItem);
    }
    else
    {
        treeItem->setIcon(0, QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    }
}

void NetworkRequestTool::populateCollectionTree()
{
    if (!m_collectionTree)
        return;

    m_collectionTree->clear();
    for (const auto &child : m_collection.root().children)
        addItemToTree(child, m_collectionTree->invisibleRootItem());

    m_collectionTree->expandAll();
}

void NetworkRequestTool::onNewCollection()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Collection", "Name:", QLineEdit::Normal, "My Collection", &ok);
    if (ok && !name.isEmpty())
    {
        m_collection = Collection(name);
        populateCollectionTree();
        saveCollection();
    }
}

void NetworkRequestTool::onAddCollectionFolder()
{
    // Determine parent: selected node or root
    QString parentId;
    if (m_collectionTree && m_collectionTree->currentItem())
        parentId = m_collectionTree->currentItem()->data(0, Qt::UserRole).toString();

    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Folder", "Name:", QLineEdit::Normal, "New Folder", &ok);
    if (ok && !name.isEmpty())
    {
        m_collection.addFolder(parentId, name);
        populateCollectionTree();
        saveCollection();
    }
}

void NetworkRequestTool::onAddCollectionRequest()
{
    QString parentId;
    if (m_collectionTree && m_collectionTree->currentItem())
        parentId = m_collectionTree->currentItem()->data(0, Qt::UserRole).toString();

    bool ok = false;
    QString name = QInputDialog::getText(this, "Add Request", "Name:",
                                          QLineEdit::Normal,
                                          ui.lineEdit_url->text().isEmpty()
                                              ? "New Request" : baseUrlFromInput(), &ok);
    if (!ok || name.isEmpty())
        return;

    // Build request JSON from current form
    QJsonObject reqJson;
    reqJson["method"] = currentMethod;
    reqJson["url"]    = ui.lineEdit_url->text();
    reqJson["body"]   = ui.textEdit_body->toPlainText();

    QJsonArray headersArr;
    for (int i = 0; i < ui.table_headers->rowCount(); ++i)
    {
        auto *k = ui.table_headers->item(i, 0);
        auto *v = ui.table_headers->item(i, 1);
        if (k && v && !k->text().isEmpty())
        {
            QJsonObject h;
            h["key"]   = k->text();
            h["value"] = v->text();
            headersArr.append(h);
        }
    }
    reqJson["headers"] = headersArr;

    // Include auth (R2)
    QJsonObject authObj;
    authObj["type"] = m_settings.authType;
    if (m_settings.authType == "Basic")
    {
        authObj["username"] = m_settings.authUsername;
        authObj["password"] = m_settings.authPassword;
    }
    else if (m_settings.authType == "Bearer")
        authObj["token"] = m_settings.authToken;
    else if (m_settings.authType == "ApiKey")
    {
        authObj["key"]      = m_settings.authApiKey;
        authObj["value"]    = m_settings.authApiValue;
        authObj["location"] = m_settings.authApiLocation;
    }
    else if (m_settings.authType == "OAuth2")
    {
        authObj["grantType"]    = m_settings.oauthGrantType;
        authObj["clientId"]     = m_settings.oauthClientId;
        authObj["clientSecret"] = m_settings.oauthClientSecret;
        authObj["tokenUrl"]     = m_settings.oauthTokenUrl;
    }
    reqJson["auth"] = authObj;

    m_collection.addRequest(parentId, name, reqJson);
    populateCollectionTree();
    saveCollection();
}

void NetworkRequestTool::onCollectionItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;

    int itemType = item->data(0, Qt::UserRole + 1).toInt();
    if (itemType != static_cast<int>(CollectionItemType::Request))
        return;

    const QString id = item->data(0, Qt::UserRole).toString();
    const CollectionItem *reqItem = m_collection.findById(id);
    if (!reqItem)
        return;

    loadRequestFromJson(reqItem->requestJson);
}

void NetworkRequestTool::onImportPostman()
{
    QString path = QFileDialog::getOpenFileName(this, "Import Postman Collection",
                                                 getDefaultDownloadDir(),
                                                 "JSON files (*.json);;All files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    bool ok = false;
    m_collection = PostmanConverter::fromPostmanV21(doc.object(), &ok);
    if (ok)
    {
        populateCollectionTree();
        saveCollection();
    }
    else
    {
        QMessageBox::warning(this, "Import Failed",
                             "Could not parse the selected file as a Postman v2.1 collection.");
    }
}

void NetworkRequestTool::onExportPostman()
{
    QJsonObject pm = PostmanConverter::toPostmanV21(m_collection);
    QJsonDocument doc(pm);

    QString path = QFileDialog::getSaveFileName(this, "Export Postman Collection",
                                                 getDefaultDownloadDir() + "/collection.json",
                                                 "JSON files (*.json);;All files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}