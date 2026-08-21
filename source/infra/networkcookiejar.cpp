#include "networkcookiejar.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>
#include <QDebug>
#include <QNetworkCookie>
#include <QDateTime>

using namespace QtNetworkRequest;

PersistentCookieJar::PersistentCookieJar(const QString &filePath, QObject *parent)
    : QNetworkCookieJar(parent), m_filePath(filePath)
{
    if (!m_filePath.isEmpty())
        load();
}

PersistentCookieJar::~PersistentCookieJar()
{
    if (!m_filePath.isEmpty())
        save();
}

void PersistentCookieJar::setFilePath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = path;
}

QString PersistentCookieJar::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

void PersistentCookieJar::load()
{
    QMutexLocker locker(&m_mutex);
    if (m_filePath.isEmpty())
        return;

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return;

    QList<QNetworkCookie> cookies;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr)
    {
        if (!val.isObject())
            continue;
        QNetworkCookie cookie = mapToCookie(val.toObject().toVariantMap());
        if (!cookie.name().isEmpty())
            cookies.append(cookie);
    }

    if (!cookies.isEmpty())
        setAllCookies(cookies);
}

void PersistentCookieJar::save()
{
    QMutexLocker locker(&m_mutex);
    if (m_filePath.isEmpty())
        return;

    QList<QNetworkCookie> cookies = allCookies();
    QJsonArray arr;
    for (const QNetworkCookie &cookie : cookies)
    {
        arr.append(QJsonObject::fromVariantMap(cookieToMap(cookie)));
    }

    QJsonDocument doc(arr);
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(doc.toJson());
    file.close();
}

QList<QNetworkCookie> PersistentCookieJar::cookiesForUrl(const QUrl &url) const
{
    QMutexLocker locker(&m_mutex);
    return QNetworkCookieJar::cookiesForUrl(url);
}

bool PersistentCookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    QMutexLocker locker(&m_mutex);
    bool changed = QNetworkCookieJar::setCookiesFromUrl(cookieList, url);
    if (changed && !m_filePath.isEmpty())
    {
        locker.unlock();
        const_cast<PersistentCookieJar*>(this)->save();
    }
    return changed;
}

QVariantMap PersistentCookieJar::cookieToMap(const QNetworkCookie &cookie)
{
    QVariantMap map;
    map["name"] = QString::fromLatin1(cookie.name());
    map["value"] = QString::fromLatin1(cookie.value());
    map["domain"] = cookie.domain();
    map["path"] = cookie.path();
    map["secure"] = cookie.isSecure();
    map["httponly"] = cookie.isHttpOnly();
    if (cookie.expirationDate().isValid())
        map["expiration"] = cookie.expirationDate().toString(Qt::ISODate);
    return map;
}

QNetworkCookie PersistentCookieJar::mapToCookie(const QVariantMap &map)
{
    QNetworkCookie cookie;
    cookie.setName(map.value("name").toString().toLatin1());
    cookie.setValue(map.value("value").toString().toLatin1());
    cookie.setDomain(map.value("domain").toString());
    cookie.setPath(map.value("path").toString());
    cookie.setSecure(map.value("secure").toBool());
    cookie.setHttpOnly(map.value("httponly").toBool());
    if (map.contains("expiration"))
    {
        QDateTime dt = QDateTime::fromString(map.value("expiration").toString(), Qt::ISODate);
        if (dt.isValid())
            cookie.setExpirationDate(dt);
    }
    return cookie;
}
