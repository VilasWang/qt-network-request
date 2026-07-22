#ifndef NETWORKREQUESTTOOL_H
#define NETWORKREQUESTTOOL_H

#include <QtWidgets/QMainWindow>
#include "ui_NetworkRequestTool.h"
#include "requestcontext.h"
#include "responseresult.h"
#include <QListWidgetItem>
#include <QDateTime>
#include <memory>

class QSyntaxHighlighter;

#ifdef QT_MTNETWORK_UNIT_TEST
class TestQtRequester;
#endif

namespace QtNetworkRequest
{

struct RequestHistory
{
    QString method;
    QString url;
    QString body;
    QMap<QString, QString> headers;
    QMap<QString, QString> params;
    QString bodyType;
    QString rawType;
    QDateTime timestamp;
};

struct RequestSettings
{
    // Proxy
    bool proxyEnabled = false;
    QString proxyHost;
    quint16 proxyPort = 8080;
    QString proxyUser;
    QString proxyPass;

    // Timeout & Retry
    int transferTimeoutMs = 30000;
    bool retryEnabled = false;
    int maxRetryCount = 3;
    int retryDelayMs = 1000;

    // Auth
    QString authType; // "None", "Basic", "Bearer"
    QString authUsername;
    QString authPassword;
    QString authToken;
};

class NetworkRequestTool : public QMainWindow
{
    Q_OBJECT

public:
    explicit NetworkRequestTool(QWidget *parent = nullptr);
    ~NetworkRequestTool();

private slots:
    void onMethodChanged(const QString &method);
    void onBodyTypeChanged(const QString &type);
    void onRawTypeChanged(const QString &type);
    void onBodyTextChanged();
    void onSendRequest();
    void onSaveRequest();
    void onNewRequest();
    void onAddParam();
    void onRemoveParam();
    void onAddHeader();
    void onRemoveHeader();
    void onSettingsClicked();
    void onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp);
    void onHistoryItemClicked(QListWidgetItem *item);
    void onSearchHistory(const QString &text);
    void onAbortTask();
    void onAbortAllTask();
    void onAddBodyParam();
    void onRemoveBodyParam();
    void onBodyTypeComboChanged(const QString &type);
    void onBodyParamTypeChanged(int row, int column);

private:
    void initialize();
    void unInitialize();
    void initializeUI();
    void initializeConnections();
    void setupDefaultValues();
    void addDefaultHeaders();
    void updateContentTypeHeader();
    void updateBodyTypeFromContentType(const QString &contentType);
    void updateHeader(const QString &key, const QString &value);
    QString buildUrlWithParams();
    QMap<QByteArray, QByteArray> getHeaders();
    QString getRequestBody();
    void applyAuthHeader();
    void applyRequestSettings(std::unique_ptr<RequestContext> &req);
    RequestType getRequestType();
    void clearResponse();
    void appendToResponse(const QString &text, const QColor &color);
    void appendToResponseBody(const QString &text, const QColor &color);
    void appendToResponseHeaders(const QString &text, const QColor &color);
    void clearResponseBody();
    void clearResponseHeaders();
    void displayResponseHeaders(const QMap<QByteArray, QByteArray> &headers);
    bool isJsonResponse(const QMap<QByteArray, QByteArray> &headers);
    bool isXmlResponse(const QMap<QByteArray, QByteArray> &headers);
    bool isOctetStreamResponse(const QMap<QByteArray, QByteArray> &headers);
    void displayJsonResponse(const QString &response);
    void saveToHistory();
    void saveToDisk(const QString &filePath);
    void loadFromDisk(const QString &filePath);
    void loadFromHistory(const RequestHistory &history);
    void updateHistoryList();
    void clearRequestForm();
    QString formatDateTime(const QDateTime &dateTime);
    QString getDefaultDownloadDir();
    QString bytesToString(qint64 bytes);
    void updateDefaultHeadersForMethod(const QString &method);
    bool isDefaultHeader(const QString &strHeader);
    QString storageDir();
    void ensureStorageDir();

private:
#ifdef QT_MTNETWORK_UNIT_TEST
    friend class ::TestQtRequester;
#endif
    Ui::networkClass ui;
    QString currentMethod;
    QString currentBodyType;
    QString currentRawType;
    QList<RequestHistory> requestHistory;
    bool isNewRequest;
    std::unique_ptr<QSyntaxHighlighter> m_highlighter;
    QString currentBoundary;
    QStringList files;
    QMap<QString, QString> kvPairs;
    QListWidgetItem *currentHistoryItem;
    RequestSettings m_settings;
    QLabel *m_labelResponseInfo;
};

} // namespace QtNetworkRequest

#endif // NETWORKREQUESTTOOL_H
