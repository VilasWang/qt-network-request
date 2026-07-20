#include "httptestserver.h"
#include <QRegularExpression>

HttpTestServer::HttpTestServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &HttpTestServer::onNewConnection);
}

HttpTestServer::~HttpTestServer() { stop(); }

bool HttpTestServer::start() { return m_server->listen(QHostAddress::LocalHost, 0); }

void HttpTestServer::stop()
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
    {
        it.key()->disconnect();
        it.key()->close();
    }
    m_buffers.clear();
    m_server->close();
}

void HttpTestServer::onNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *s = m_server->nextPendingConnection();
        connect(s, &QTcpSocket::readyRead, this, &HttpTestServer::onReadyRead);
        connect(s, &QTcpSocket::disconnected, this, &HttpTestServer::onDisconnected);
        m_buffers[s] = QByteArray();
    }
}

void HttpTestServer::onReadyRead()
{
    QTcpSocket *s = qobject_cast<QTcpSocket*>(sender());
    if (!s || !m_buffers.contains(s)) return;

    m_buffers[s].append(s->readAll());
    QByteArray &buf = m_buffers[s];
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd == -1) return;

    // Parse request line
    QList<QByteArray> lines = buf.left(headerEnd).split('\n');
    if (lines.isEmpty()) { s->close(); return; }

    QList<QByteArray> rl = lines[0].trimmed().split(' ');
    if (rl.size() < 2) { s->close(); return; }

    HttpRequest req;
    req.method = QString::fromUtf8(rl[0]).toUpper();

    QString rawPath = QString::fromUtf8(rl[1]);
    int qm = rawPath.indexOf('?');
    if (qm != -1)
    {
        req.path = rawPath.left(qm);
        req.query = QUrlQuery(rawPath.mid(qm + 1));
    }
    else
    {
        req.path = rawPath;
    }

    for (int i = 1; i < lines.size(); ++i)
    {
        QString line = QString::fromUtf8(lines[i]).trimmed();
        int colon = line.indexOf(':');
        if (colon != -1)
        {
            QString key = line.left(colon).trimmed().toLower();
            QString value = line.mid(colon + 1).trimmed();
            req.headers.insert(key, value);
        }
    }

    if (req.headers.contains("content-length"))
    {
        int cl = req.headers["content-length"].toInt();
        int bodyStart = headerEnd + 4;
        if (buf.size() - bodyStart < cl) return;
        req.body = buf.mid(bodyStart, cl);
    }

    processRequest(s, req);
}

void HttpTestServer::onDisconnected()
{
    QTcpSocket *s = qobject_cast<QTcpSocket*>(sender());
    if (s) { m_buffers.remove(s); s->deleteLater(); }
}

void HttpTestServer::processRequest(QTcpSocket *s, const HttpRequest &req)
{
    // Transient failure simulation: partial response then drop
    if (!m_failPath.isEmpty() && req.path == m_failPath && m_failCount > 0)
    {
        m_failCount--;
        s->write("HTTP/1.1 200 OK\r\n");
        s->write("Content-Length: 99999\r\n");
        s->write("Connection: close\r\n\r\n");
        s->write(QByteArray(500, 'X'));
        s->flush();
        s->disconnectFromHost();
        return;
    }

    HttpResponse resp = routeRequest(req);
    s->write(buildResponse(resp));
    s->flush();
    s->disconnectFromHost();
}

HttpTestServer::HttpResponse HttpTestServer::routeRequest(const HttpRequest &req)
{
    if (req.method == "GET" && req.path == "/cookies/set")
        return handleCookiesSet(req);
    if (req.method == "GET" && req.path == "/cookies")
        return handleCookies(req);
    if (req.method == "GET" && req.path.startsWith("/bytes/"))
        return handleBytes(req);
    if (req.method == "GET")
        return handleEchoGet(req);
    if (req.method == "HEAD" && req.path.startsWith("/bytes/"))
    {
        HttpResponse resp = handleBytes(req);
        resp.body.clear(); // HEAD has no body
        return resp;
    }
    if (req.method == "HEAD")
        return handleHead(req);
    if (req.method == "POST")
        return handlePost(req);
    if (req.method == "PUT")
        return handlePut(req);
    if (req.method == "DELETE")
        return handleDelete(req);
    return handleFourOhFour();
}

void HttpTestServer::HttpResponse::setJsonBody(const QJsonObject &obj)
{
    body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    setContentType("application/json");
    headers["Content-Length"] = QString::number(body.size());
}

HttpTestServer::HttpResponse HttpTestServer::handleEchoGet(const HttpRequest &req)
{
    QJsonObject json;
    json["url"] = QString("http://127.0.0.1:%1%2").arg(port()).arg(req.path);
    json["method"] = "GET";

    QJsonObject hdrs;
    for (auto it = req.headers.begin(); it != req.headers.end(); ++it)
        hdrs[it.key()] = it.value();
    json["headers"] = hdrs;

    QJsonObject args;
    for (const auto &item : req.query.queryItems())
        args[item.first] = item.second;
    json["args"] = args;

    HttpResponse resp;
    resp.setJsonBody(json);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handlePost(const HttpRequest &req)
{
    QJsonObject json;
    json["url"] = QString("http://127.0.0.1:%1%2").arg(port()).arg(req.path);
    json["method"] = "POST";
    json["data"] = QString::fromUtf8(req.body);

    QJsonObject hdrs;
    for (auto it = req.headers.begin(); it != req.headers.end(); ++it)
        hdrs[it.key()] = it.value();
    json["headers"] = hdrs;

    HttpResponse resp;
    resp.setJsonBody(json);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handlePut(const HttpRequest &req)
{
    QJsonObject json;
    json["url"] = QString("http://127.0.0.1:%1%2").arg(port()).arg(req.path);
    json["method"] = "PUT";
    json["data"] = QString::fromUtf8(req.body);

    QJsonObject hdrs;
    for (auto it = req.headers.begin(); it != req.headers.end(); ++it)
        hdrs[it.key()] = it.value();
    json["headers"] = hdrs;

    HttpResponse resp;
    resp.setJsonBody(json);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handleDelete(const HttpRequest &req)
{
    QJsonObject json;
    json["url"] = QString("http://127.0.0.1:%1%2").arg(port()).arg(req.path);
    json["method"] = "DELETE";

    QJsonObject hdrs;
    for (auto it = req.headers.begin(); it != req.headers.end(); ++it)
        hdrs[it.key()] = it.value();
    json["headers"] = hdrs;

    HttpResponse resp;
    resp.setJsonBody(json);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handleHead(const HttpRequest &)
{
    HttpResponse resp;
    resp.headers["Content-Type"] = "application/json";
    return resp;
}

int HttpTestServer::parseRange(const QString &range, qint64 total, qint64 &start, qint64 &end)
{
    QString lc = range.toLower();
    if (!lc.startsWith("bytes=")) return -1;
    QString rv = range.mid(6);
    int dash = rv.indexOf('-');
    if (dash == -1) return -1;

    bool ok1 = false, ok2 = false;
    start = rv.left(dash).trimmed().toLongLong(&ok1);
    QString es = rv.mid(dash + 1).trimmed();
    end = es.isEmpty() ? total - 1 : es.toLongLong(&ok2);
    return (ok1 && (es.isEmpty() || ok2)) ? 0 : -1;
}

HttpTestServer::HttpResponse HttpTestServer::handleBytes(const HttpRequest &req)
{
    QStringList parts = req.path.split('/');
    if (parts.size() < 3) return handleFourOhFour();
    bool ok = false;
    qint64 total = parts[2].toLongLong(&ok);
    if (!ok || total <= 0) return handleFourOhFour();

    if (!req.headers.contains("range"))
    {
        HttpResponse resp;
        resp.body = QByteArray(total, 'A');
        resp.headers["Content-Type"] = "application/octet-stream";
        resp.headers["Content-Length"] = QString::number(total);
        return resp;
    }

    qint64 start = 0, end = 0;
    if (parseRange(req.headers["range"], total, start, end) != 0)
        return handleFourOhFour();

    qint64 len = end - start + 1;
    HttpResponse resp;
    resp.statusCode = 206;
    resp.statusText = "Partial Content";
    resp.body = QByteArray(len, 'A');
    resp.headers["Content-Type"] = "application/octet-stream";
    resp.headers["Content-Length"] = QString::number(len);
    resp.headers["Content-Range"] = QString("bytes %1-%2/%3").arg(start).arg(end).arg(total);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handleCookies(const HttpRequest &req)
{
    QJsonObject cookiesJson;
    if (req.headers.contains("cookie"))
    {
        QString cookieHeader = req.headers["cookie"];
        QStringList pairs = cookieHeader.split(';');
        for (const QString &pair : pairs)
        {
            int eq = pair.indexOf('=');
            if (eq != -1)
                cookiesJson[pair.left(eq).trimmed()] = pair.mid(eq + 1).trimmed();
        }
    }

    QJsonObject json;
    json["cookies"] = cookiesJson;

    HttpResponse resp;
    resp.setJsonBody(json);
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handleCookiesSet(const HttpRequest &req)
{
    QStringList setCookieHeaders;
    for (const auto &item : req.query.queryItems())
    {
        setCookieHeaders << QString("%1=%2; Path=/").arg(item.first, item.second);
    }

    HttpResponse resp;
    resp.headers["Set-Cookie"] = setCookieHeaders.join(", ");
    resp.setJsonBody(QJsonObject{{"status", "ok"}});
    return resp;
}

HttpTestServer::HttpResponse HttpTestServer::handleFourOhFour()
{
    HttpResponse resp;
    resp.statusCode = 404;
    resp.statusText = "Not Found";
    resp.setContentType("text/plain");
    resp.body = QByteArray("Not Found");
    resp.headers["Content-Length"] = QByteArray::number(resp.body.size());
    return resp;
}

QByteArray HttpTestServer::buildResponse(const HttpResponse &resp)
{
    QByteArray result;
    result += QString("HTTP/1.1 %1 %2\r\n").arg(resp.statusCode).arg(resp.statusText).toUtf8();

    for (auto it = resp.headers.begin(); it != resp.headers.end(); ++it)
    {
        result += QString("%1: %2\r\n").arg(it.key(), it.value()).toUtf8();
    }

    if (!resp.headers.contains("Content-Length") && !resp.body.isEmpty())
    {
        result += QString("Content-Length: %1\r\n").arg(resp.body.size()).toUtf8();
    }
    result += "\r\n";
    result += resp.body;
    return result;
}
