#ifndef NETWORKREQUESTTOOL_H
#define NETWORKREQUESTTOOL_H

#include <QtWidgets/QMainWindow>
#include "ui_NetworkRequestTool.h"
#include "requestcontext.h"
#include "authconfig.h"
#include "environmentstore.h"
#include "collectionmodel.h"
#include "responseresult.h"
#include <QListWidgetItem>
#include <QDateTime>
#include <memory>

class QSyntaxHighlighter;
class QTreeWidget;
class QTreeWidgetItem;

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
    QString authType; // "None", "Basic", "Bearer", "ApiKey"
    QString authUsername;
    QString authPassword;
    QString authToken;
    QString authApiKey;
    QString authApiValue;
    QString authApiLocation; // "Header" / "Query"

    // OAuth2 (M2)
    QString oauthGrantType; // "ClientCredentials" / "Password" / "RefreshToken"
    QString oauthClientId;
    QString oauthClientSecret;
    QString oauthScopes;
    QString oauthTokenUrl;
    QString oauthUsername;
    QString oauthPassword;
    QString oauthRefreshToken;
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
    void onEnvironmentChanged(int index);
    void onManageEnvironments();
    void onResponseSearchChanged(const QString &text);
    void onResponseFindPrev();
    void onResponseFindNext();
    void onResponseCopy();
    void onResponseSave();
    void onResponsePrettyToggled(bool checked);

    // Collection (M4)
    void onCollectionItemClicked(QTreeWidgetItem *item, int column);
    void onNewCollection();
    void onAddCollectionFolder();
    void onAddCollectionRequest();
    void onImportPostman();
    void onExportPostman();

    void onResponse(QSharedPointer<QtNetworkRequest::ResponseResult> rsp);
    void onHistoryItemClicked(QListWidgetItem *item);
    void onSearchHistory(const QString &text);
    void onAbortTask();
    void onAbortAllTask();
    void onAddBodyParam();
    void onRemoveBodyParam();
    void onBodyParamTypeChanged(int row, int column);

private:
    void initialize();
    void unInitialize();
    void initializeUI();
    void initializeConnections();
    void setupDefaultValues();
    void addDefaultHeaders();
    void updateContentTypeHeader();
    void applyBodySyntaxHighlighting(const QString &rawType);
    void updateBodyTypeFromContentType(const QString &contentType);
    void updateHeader(const QString &key, const QString &value);
    QString buildUrlWithParams();
    QString baseUrlFromInput() const;
    QMap<QByteArray, QByteArray> getHeaders();
    QString getRequestBody();
    QMap<QString, QString> getQueryParams() const;
    QMap<QString, QString> getFormUrlEncodedMap() const;
    QByteArray readBinaryFile(const QString &path) const;
    void applyAuthHeader();
    AuthConfig buildAuthConfig() const;
    std::unique_ptr<RequestContext> buildRequestContext();
    void applyRequestSettings(std::unique_ptr<RequestContext> &req);
    RequestType getRequestType();
    void clearResponse();
    void appendToResponse(const QString &text, const QColor &color);
    void appendToResponseBody(const QString &text, const QColor &color);
    void appendToResponseHeaders(const QString &text, const QColor &color);
    void clearResponseBody();
    void clearResponseHeaders();
    void displayResponseHeaders(const QMap<QByteArray, QByteArray> &headers);
    void displayResponseCookies(const QList<QNetworkCookie> &cookies);
    void clearResponseCookies();
    bool isJsonResponse(const QMap<QByteArray, QByteArray> &headers);
    bool isXmlResponse(const QMap<QByteArray, QByteArray> &headers);
    bool isOctetStreamResponse(const QMap<QByteArray, QByteArray> &headers);
    void displayJsonResponse(const QString &response);
    void saveToHistory();
    void saveToDisk(const QString &filePath);
    void loadFromDisk(const QString &filePath);
    void loadRequestFromJson(const QJsonObject &obj);
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
    void loadEnvironments();
    void saveEnvironments();
    void populateEnvironmentCombo();
    void buildResponseToolbar();
    void buildCollectionPanel();
    void saveCollection();
    void loadCollection();
    void populateCollectionTree();
    void doResponseSearch();
    void navigateSearchHit(int delta);

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
    EnvironmentStore m_envStore;
    QComboBox *m_cmbEnvironment{nullptr};
    QPushButton *m_btnManageEnv{nullptr};
    QLabel *m_labelResponseInfo;
    QWidget *m_responseToolbar{nullptr};
    QLineEdit *m_leResponseSearch{nullptr};
    QString m_lastResponseBody;      // raw body for pretty/raw toggle
    bool m_isResponseJson{false};    // set when the response is JSON
    QList<QTextEdit::ExtraSelection> m_searchSelections;
    int m_currentSearchHit{-1};
    QString m_binaryFilePath;

    // Collection (M4)
    QTreeWidget *m_collectionTree{nullptr};
    Collection m_collection;             // in-memory collection model
    QString m_collectionPath;            // file path for persistence
};

} // namespace QtNetworkRequest

#endif // NETWORKREQUESTTOOL_H
