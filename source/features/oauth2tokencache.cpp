/*
@Brief:			Qt multi-threaded network request module – OAuth2 token cache

MIT License

Copyright (c) 2025 Lucas Wang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "oauth2tokencache.h"

#include <QGlobalStatic>

namespace QtNetworkRequest
{
	Q_GLOBAL_STATIC(OAuth2TokenCache, g_oauth2TokenCache)

	OAuth2TokenCache &OAuth2TokenCache::instance()
	{
		return *g_oauth2TokenCache;
	}

	QString OAuth2TokenCache::makeKey(const QString &tokenUrl, const QString &clientId,
	                                  const QString &grant, const QString &scopes)
	{
		return tokenUrl + QLatin1Char('|') + clientId + QLatin1Char('|')
		       + grant + QLatin1Char('|') + scopes;
	}

	bool OAuth2TokenCache::find(const QString &key, OAuth2TokenEntry &out) const
	{
		QMutexLocker locker(&m_mutex);
		auto it = m_entries.constFind(key);
		if (it == m_entries.constEnd())
			return false;
		out = it.value();
		return true;
	}

	void OAuth2TokenCache::store(const QString &key, const OAuth2TokenEntry &entry)
	{
		QMutexLocker locker(&m_mutex);
		m_entries.insert(key, entry);
	}

	void OAuth2TokenCache::clear()
	{
		QMutexLocker locker(&m_mutex);
		m_entries.clear();
	}
}
