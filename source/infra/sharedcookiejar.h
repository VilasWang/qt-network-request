#pragma once

#include <QNetworkCookieJar>
#include <QNetworkCookie>

namespace QtNetworkRequest
{
	// Lightweight delegate that forwards all cookie operations to a shared
	// (non-owning) global jar.  Each QNetworkAccessManager in the pool gets
	// its own SharedCookieJar instance; when the NAM is destroyed this
	// wrapper is deleted but the real underlying jar is untouched.
	class SharedCookieJar : public QNetworkCookieJar
	{
		// No Q_OBJECT — this class only overrides virtual methods
		// and does not emit signals. Using Q_OBJECT on a header-only
		// class causes meta-object lifecycle issues when the object
		// is created in a worker thread but deleted from the main thread.

	public:
		explicit SharedCookieJar(QNetworkCookieJar *shared, QObject *parent = nullptr)
			: QNetworkCookieJar(parent), m_shared(shared)
		{
		}

		QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override
		{
			return m_shared ? m_shared->cookiesForUrl(url) : QList<QNetworkCookie>();
		}

		bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override
		{
			return m_shared ? m_shared->setCookiesFromUrl(cookieList, url) : false;
		}

		bool insertCookie(const QNetworkCookie &cookie) override
		{
			return m_shared ? m_shared->insertCookie(cookie) : false;
		}

		bool updateCookie(const QNetworkCookie &cookie) override
		{
			return m_shared ? m_shared->updateCookie(cookie) : false;
		}

		bool deleteCookie(const QNetworkCookie &cookie) override
		{
			return m_shared ? m_shared->deleteCookie(cookie) : false;
		}

		// Exposed for the pool to avoid recreating wrappers unnecessarily
		QNetworkCookieJar *sharedJar() const { return m_shared; }

	private:
		QNetworkCookieJar *m_shared; // non-owning reference to the global jar
	};
}
