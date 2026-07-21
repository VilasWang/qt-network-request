#ifndef HTTPTESTSERVER_H
#define HTTPTESTSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class HttpTestServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpTestServer(QObject *parent = nullptr);
    ~HttpTestServer();

    bool start();
    void stop();
    quint16 port() const { return m_server->serverPort(); }
    QString baseUrl() const { return QString("http://127.0.0.1:%1").arg(port()); }

    void setFailPath(const QString &path, int count = 1)
        { m_failPath = path; m_failCount = count; }
    bool allRetriesPassed() const { return m_failCount == 0; }

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct HttpRequest {
        QString method;
        QString path;
        QUrlQuery query;
        QMap<QString, QString> headers;
        QByteArray body;
    };

    struct HttpResponse {
        int statusCode = 200;
        QString statusText = "OK";
        QMap<QString, QString> headers;
        QByteArray body;

        void setContentType(const QString &t) { headers["Content-Type"] = t; }
        void setJsonBody(const QJsonObject &obj);
    };

    void processRequest(QTcpSocket *socket, const HttpRequest &req);
    HttpResponse routeRequest(const HttpRequest &req);
    HttpResponse handleEchoGet(const HttpRequest &req);
    HttpResponse handlePost(const HttpRequest &req);
    HttpResponse handlePut(const HttpRequest &req);
    HttpResponse handleDelete(const HttpRequest &req);
    HttpResponse handleHead(const HttpRequest &req);
    HttpResponse handleBytes(const HttpRequest &req);
    HttpResponse handleCookies(const HttpRequest &req);
    HttpResponse handleCookiesSet(const HttpRequest &req);
    HttpResponse handleDelay(const HttpRequest &req);
    HttpResponse handleFourOhFour();
    QByteArray buildResponse(const HttpResponse &resp);
    int parseRange(const QString &range, qint64 total, qint64 &start, qint64 &end);

    QTcpServer *m_server;
    QMap<QTcpSocket*, QByteArray> m_buffers;
    QString m_failPath;
    int m_failCount = 0;
};

#endif // HTTPTESTSERVER_H
