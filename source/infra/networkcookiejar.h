#pragma once

#include <QNetworkCookieJar>
#include <QMutex>
#include <QString>

namespace QtNetworkRequest
{
    class PersistentCookieJar : public QNetworkCookieJar
    {
        Q_OBJECT

    public:
        explicit PersistentCookieJar(const QString &filePath = QString(), QObject *parent = nullptr);
        ~PersistentCookieJar();

        void setFilePath(const QString &path);
        QString filePath() const;

        void load();
        void save();

        QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const Q_DECL_OVERRIDE;
        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) Q_DECL_OVERRIDE;

    private:
        static QVariantMap cookieToMap(const QNetworkCookie &cookie);
        static QNetworkCookie mapToCookie(const QVariantMap &map);

        mutable QMutex m_mutex;
        QString m_filePath;
    };
}
