#include "requeststrategies.h"
#include "networkrequest.h"
#include <QNetworkReply>

namespace QtNetworkRequest
{

// ============================================================================
// DefaultRetryStrategy
// ============================================================================
bool DefaultRetryStrategy::shouldRetry(QNetworkReply::NetworkError err,
										int attemptCount,
										int maxRetries) const
{
	return attemptCount < maxRetries && NetworkRequest::isTransientError(err);
}

int DefaultRetryStrategy::retryDelayMs(int attemptCount) const
{
	// exponential backoff: baseDelayMs * 2^(attemptCount-1), capped at maxDelayMs
	return qMin(baseDelayMs * (1 << (attemptCount - 1)), maxDelayMs);
}

// ============================================================================
// DefaultRedirectHandler
// ============================================================================
bool DefaultRedirectHandler::shouldRedirect(int httpStatusCode,
											 int redirectCount,
											 int maxRedirects) const
{
	return (httpStatusCode == 301 || httpStatusCode == 302) && redirectCount < maxRedirects;
}

QUrl DefaultRedirectHandler::resolveRedirect(const QUrl& current, const QUrl& target) const
{
	return current.resolved(target);
}

// ============================================================================
// DefaultSslPolicy
// ============================================================================
#ifndef QT_NO_SSL
SslActionResult DefaultSslPolicy::evaluate(const QList<QSslError>& errors,
											SslConfig::IgnorePolicy policy,
											const QList<QSslError::SslError>& ignorableTypes) const
{
	switch (policy)
	{
	case SslConfig::IgnorePolicy::Always:
		return SslActionResult::Ignore;

	case SslConfig::IgnorePolicy::IgnoreSpecificErrors:
	{
		for (const QSslError& e : errors)
		{
			if (!ignorableTypes.contains(e.error()))
				return SslActionResult::Reject;
		}
		return SslActionResult::Accept;
	}

	case SslConfig::IgnorePolicy::Never:
	case SslConfig::IgnorePolicy::Inherit:
	default:
		return SslActionResult::Reject;
	}
}
#endif // QT_NO_SSL

} // namespace QtNetworkRequest
