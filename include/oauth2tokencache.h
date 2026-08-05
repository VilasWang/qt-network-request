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

#pragma once

#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QMap>
#include "networkrequestglobal.h"

namespace QtNetworkRequest
{
	/// A cached OAuth2 token pair with an absolute expiry timestamp.
	struct NETWORK_EXPORT OAuth2TokenEntry
	{
		QString accessToken;
		QString refreshToken;
		QDateTime expiresAt;   // absolute expiry; isExpired() compares to now()

		bool isExpired() const { return QDateTime::currentDateTime() >= expiresAt; }
		bool isValid() const { return !accessToken.isEmpty() && !isExpired(); }
	};

	/// Process-wide, thread-safe OAuth2 token cache.
	///
	/// Tokens are cached in memory only (never persisted to disk) and keyed by
	/// tokenUrl|clientId|grant|scopes. A single QMutex guards all reads/writes so
	/// the cache is safe to share across the worker threads that run requests.
	class NETWORK_EXPORT OAuth2TokenCache
	{
	public:
		OAuth2TokenCache() = default;
		Q_DISABLE_COPY(OAuth2TokenCache)

		/// Process-wide singleton (tokens survive across requests and managers).
		static OAuth2TokenCache &instance();

		/// Build a stable cache key from the identifying grant parameters.
		static QString makeKey(const QString &tokenUrl, const QString &clientId,
		                       const QString &grant, const QString &scopes);

		/// Look up a cached entry. Returns false if absent. `out` is filled on success.
		bool find(const QString &key, OAuth2TokenEntry &out) const;
		/// Store (or replace) a cached entry.
		void store(const QString &key, const OAuth2TokenEntry &entry);
		/// Drop all cached entries (e.g. on logout / config change).
		void clear();

	private:
		mutable QMutex m_mutex;
		QMap<QString, OAuth2TokenEntry> m_entries;
	};
}
