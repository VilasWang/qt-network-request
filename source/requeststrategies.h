#pragma once

/// @file requeststrategies.h
/// @brief Strategy interfaces + default implementations for pluggable request behavior.
///
/// All behavioral policies (retry, redirect, SSL, timeout, progress throttling)
/// are extracted into strategy interfaces so that:
///   1. Each behavior has a single implementation across all request types
///   2. Strategies are independently unit-testable
///   3. Custom strategies can be injected at runtime (e.g. test-mode no-retry)

#include <QList>
#include <QUrl>
#include <QNetworkReply>
#include "networkerror.h"
#include "sslconfig.h"

#ifndef QT_NO_SSL
#include <QSslError>
#endif

class QNetworkAccessManager;

namespace QtNetworkRequest
{

// ============================================================================
// IRetryStrategy — decide whether and when to retry a failed request
// ============================================================================
struct IRetryStrategy
{
	virtual ~IRetryStrategy() = default;

	/// @return true if the error is transient and retry count permits
	virtual bool shouldRetry(QNetworkReply::NetworkError err,
							 int attemptCount,
							 int maxRetries) const = 0;

	/// @return delay in milliseconds before the next attempt
	virtual int retryDelayMs(int attemptCount) const = 0;
};

/// Default: exponential backoff, transient errors only
class DefaultRetryStrategy : public IRetryStrategy
{
public:
	bool shouldRetry(QNetworkReply::NetworkError err,
					 int attemptCount,
					 int maxRetries) const override;

	int retryDelayMs(int attemptCount) const override;

	int baseDelayMs = 1000;   ///< initial delay (ms)
	int maxDelayMs  = 30000;  ///< cap delay at 30 s
};


// ============================================================================
// IRedirectHandler — handle 301/302 redirects
// ============================================================================
struct IRedirectHandler
{
	virtual ~IRedirectHandler() = default;

	/// @return true if the redirect should be followed
	virtual bool shouldRedirect(int httpStatusCode,
								int redirectCount,
								int maxRedirects) const = 0;

	/// Resolve a relative redirect target against the current URL
	virtual QUrl resolveRedirect(const QUrl& current, const QUrl& target) const = 0;
};

class DefaultRedirectHandler : public IRedirectHandler
{
public:
	bool shouldRedirect(int httpStatusCode,
						int redirectCount,
						int maxRedirects) const override;

	QUrl resolveRedirect(const QUrl& current, const QUrl& target) const override;
};

// ============================================================================
// ISslPolicy — handle SSL certificate errors
// ============================================================================
#ifndef QT_NO_SSL
enum class SslActionResult { Accept, Reject, Ignore };

struct ISslPolicy
{
	virtual ~ISslPolicy() = default;
	virtual SslActionResult evaluate(const QList<QSslError>& errors,
									  SslConfig::IgnorePolicy policy,
									  const QList<QSslError::SslError>& ignorableTypes) const = 0;
};

class DefaultSslPolicy : public ISslPolicy
{
public:
	SslActionResult evaluate(const QList<QSslError>& errors,
							  SslConfig::IgnorePolicy policy,
							  const QList<QSslError::SslError>& ignorableTypes) const override;
};
#endif // QT_NO_SSL

// ============================================================================
// IProgressThrottle — fire progress events at most once per interval
// ============================================================================
// 注: ProgressThrottle 是完整的 QObject 组件，位于 progressthrottle.h。
//     此接口供 Phase 2 正式引入，Phase 1 仅预留。

// ============================================================================
// ITimeoutStrategy — idle / transfer / total timeout calculations
// ============================================================================
struct ITimeoutStrategy
{
	virtual ~ITimeoutStrategy() = default;
	virtual int idleTimeoutMs()     const = 0;
	virtual int transferTimeoutMs() const = 0;
	virtual int totalTimeoutMs()    const = 0;
};

} // namespace QtNetworkRequest
