#pragma once

#include <QObject>
#include <functional>
#include "networkrequest.h"

namespace QtNetworkRequest
{
	// Common request
	class NetworkCommonRequest : public NetworkRequest
	{
		Q_OBJECT;

	public:
		explicit NetworkCommonRequest(QObject *parent = 0);
		~NetworkCommonRequest();

	protected:
		void cleanupForRetry() Q_DECL_OVERRIDE {}

	public Q_SLOTS:
		void start() Q_DECL_OVERRIDE;
		void onFinished() Q_DECL_OVERRIDE;

	private:
		/// Send the actual HTTP request (extracted from start() so OAuth2
		/// can call it from the token-fetch continuation).
		void performSend();

		/// Fetch an OAuth2 access token asynchronously, then call onReady.
		/// Uses OAuth2TokenCache for the fast path (cached token within expires_in).
		void fetchOAuth2Token(std::function<void()> onReady);
	};
}
