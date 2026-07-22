#include "networkaccessmanagerpool.h"
#include "sharedcookiejar.h"
#include "networkrequestmanager.h"
#include <QDebug>

using namespace QtNetworkRequest;

NetworkAccessManagerPool::NetworkAccessManagerPool()
{
	qDebug() << "[QMultiThreadNetwork] NAM pool created";
}

NetworkAccessManagerPool::~NetworkAccessManagerPool()
{
	releaseAll();
}

QNetworkAccessManager *NetworkAccessManagerPool::acquireNam()
{
	Qt::HANDLE threadId = QThread::currentThreadId();
	QThread *currentThread = QThread::currentThread();

	// --- fast path: check with lock held ---
	{
		QMutexLocker locker(&m_mutex);
		auto it = m_namPool.find(threadId);
		if (it != m_namPool.end() && it->thread == currentThread)
		{
			QNetworkAccessManager *nam = it->nam;

			// Hot-update: re-apply global proxy on every acquisition
			const ProxyConfig &proxy = NetworkRequestManager::globalProxy();
			if (proxy.enabled)
				nam->setProxy(proxy.toQNetworkProxy());
			else
				nam->setProxy(QNetworkProxy::DefaultProxy);

			// Hot-update: rebuild SharedCookieJar wrapper only when needed.
			// Skip recreation if the current jar is already a SharedCookieJar
			// delegating to the same global jar (avoids unnecessary allocs).
			QNetworkCookieJar *globalJar = NetworkRequestManager::cookieJar();
			{
				auto *existingJar = nam->cookieJar();
				auto *sharedJar = dynamic_cast<SharedCookieJar *>(existingJar);
				bool needsUpdate = !sharedJar || sharedJar->sharedJar() != globalJar;
				if (needsUpdate)
				{
					if (globalJar)
						nam->setCookieJar(new SharedCookieJar(globalJar));
					else
						nam->setCookieJar(nullptr);
				}
			}

			return nam;
		}
	}
	// lock released here - expensive allocation done without holding lock

	// --- slow path: create a new NAM for this thread ---
	auto *nam = new QNetworkAccessManager(); // no parent - pool owns lifecycle

	// --- double-check: another thread might have created one while we were unlocked ---
	{
		QMutexLocker locker(&m_mutex);
		auto it = m_namPool.find(threadId);
		if (it != m_namPool.end() && it->thread == currentThread)
		{
			// Race lost - discard our newly-created NAM and use the existing one
			delete nam;
			nam = it->nam;
		}
		else
		{
			m_namPool.insert(threadId, { nam, currentThread });
			qDebug() << "[QMultiThreadNetwork] NAM pool: created NAM for thread"
					 << threadId << ", pool size:" << m_namPool.size();
		}
	}

	// Apply initial proxy configuration
	{
		const ProxyConfig &proxy = NetworkRequestManager::globalProxy();
		if (proxy.enabled)
			nam->setProxy(proxy.toQNetworkProxy());
		else
			nam->setProxy(QNetworkProxy::DefaultProxy);
	}

	// Inject SharedCookieJar wrapper if not already present
	{
		QNetworkCookieJar *globalJar = NetworkRequestManager::cookieJar();
		auto *existingJar = nam->cookieJar();
		auto *sharedJar = dynamic_cast<SharedCookieJar *>(existingJar);
		bool needsUpdate = !sharedJar || sharedJar->sharedJar() != globalJar;
		if (needsUpdate)
		{
			if (globalJar)
				nam->setCookieJar(new SharedCookieJar(globalJar));
			else
				nam->setCookieJar(nullptr);
		}
	}

	return nam;
}

void NetworkAccessManagerPool::releaseAll()
{
	QMutexLocker locker(&m_mutex);
	if (m_namPool.isEmpty())
		return;

	qDebug() << "[QMultiThreadNetwork] NAM pool: releasing" << m_namPool.size() << "NAM(s)";
	for (auto it = m_namPool.begin(); it != m_namPool.end(); ++it)
	{
		delete it->nam;
	}
	m_namPool.clear();
}

int NetworkAccessManagerPool::size() const
{
	QMutexLocker locker(&m_mutex);
	return m_namPool.size();
}
