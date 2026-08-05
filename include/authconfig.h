/*
@Brief:			Qt multi-threaded network request module – authentication configuration

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
#include <QByteArray>

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
	/// Supported authentication types (extensible for future Digest, OAuth2, etc.)
	enum class AuthType : int8_t
	{
		None   = 0,  // No authentication
		Basic  = 1,  // HTTP Basic Authentication (RFC 7617)
		Bearer = 2,  // Bearer Token Authentication (RFC 6750)
		ApiKey = 3,  // Custom API Key in Header or Query Param
		OAuth2 = 4,  // OAuth 2.0 (client-credentials / password / refresh-token)
	};

	/// Placement strategy for API Key authentication
	enum class ApiKeyPlacement : int8_t
	{
		Header     = 0,  // Add as custom request header
		QueryParam = 1,  // Append to URL query string
	};

	/// OAuth 2.0 grant types supported by AuthConfig::OAuth2.
	/// (Authorization Code + PKCE is intentionally out of scope for v1.)
	enum class OAuth2GrantType : int8_t
	{
		ClientCredentials = 0,  // client_credentials
		Password          = 1,  // Resource Owner Password Credentials (ROPC)
		RefreshToken      = 2,  // refresh_token (also used for auto-renew)
	};

	/// Authentication configuration (value type, read-only after RequestContext build)
	struct AuthConfig
	{
		AuthType type{ AuthType::None };

		// --- Basic Auth ---
		QString username;
		QString password;

		// --- Bearer Token ---
		/// Raw token value. The library auto-prepends "Bearer " prefix when building the header.
		QString token;

		// --- API Key ---
		QString apiKey;     // Header name or query param key
		QString apiValue;   // Header value or query param value
		ApiKeyPlacement apiKeyPlacement{ ApiKeyPlacement::Header };

		/// OAuth 2.0 grant configuration (client-credentials / password / refresh-token).
		struct OAuth2Config
		{
			OAuth2GrantType grant{ OAuth2GrantType::ClientCredentials };
			QString clientId;
			QString clientSecret;
			QString scopes;          // space-separated scope list
			QString tokenUrl;        // absolute token endpoint
			// Password grant
			QString username;
			QString password;
			// Refresh grant (also used for auto-renew of any grant)
			QString refreshToken;
		};

		// --- OAuth2 ---
		OAuth2Config oauth2Config;

		// --- Factory methods ---

		/// Create a Basic Authentication config.
		/// @param user Username (must not be empty)
		/// @param pass Password
		static AuthConfig basic(const QString &user, const QString &pass)
		{
			AuthConfig c;
			c.type = AuthType::Basic;
			c.username = user;
			c.password = pass;
			return c;
		}

		/// Create a Bearer Token config.
		/// @param tok Raw token string (must not be empty)
		static AuthConfig bearer(const QString &tok)
		{
			AuthConfig c;
			c.type = AuthType::Bearer;
			c.token = tok;
			return c;
		}

		/// Create an API Key config.
		/// @param key   Header name or query param key (must not be empty)
		/// @param value Header value or query param value (must not be empty)
		/// @param placement Where to inject the key (default: Header)
		static AuthConfig apiKeyAuth(const QString &key, const QString &value,
		                         ApiKeyPlacement placement = ApiKeyPlacement::Header)
		{
			AuthConfig c;
			c.type = AuthType::ApiKey;
			c.apiKey = key;
			c.apiValue = value;
			c.apiKeyPlacement = placement;
			return c;
		}

		/// Create an OAuth 2.0 config from a grant configuration.
		static AuthConfig oauth2(const OAuth2Config &cfg)
		{
			AuthConfig c;
			c.type = AuthType::OAuth2;
			c.oauth2Config = cfg;
			return c;
		}

		/// Validate that required credentials are present for the selected auth type.
		/// AuthType::None is always considered valid.
		bool isValid() const
		{
			switch (type)
			{
			case AuthType::Basic:  return !username.isEmpty();
			case AuthType::Bearer: return !token.isEmpty();
			case AuthType::ApiKey: return !apiKey.isEmpty() && !apiValue.isEmpty();
			case AuthType::OAuth2:
				if (oauth2Config.grant == OAuth2GrantType::Password)
					return !oauth2Config.tokenUrl.isEmpty() && !oauth2Config.clientId.isEmpty()
					       && !oauth2Config.username.isEmpty() && !oauth2Config.password.isEmpty();
				return !oauth2Config.tokenUrl.isEmpty() && !oauth2Config.clientId.isEmpty();
			case AuthType::None:   return true;
			default:               return false;
			}
		}

		/// Generate the Authorization header value.
		/// For Basic:  base64-encoded "user:pass"
		/// For Bearer: "Bearer <token>"
		/// For others: empty QByteArray
		QByteArray authorizationHeaderValue() const
		{
			switch (type)
			{
			case AuthType::Basic:
			{
				QByteArray credentials = (username + u':' + password).toUtf8();
				return QByteArrayLiteral("Basic ") + credentials.toBase64();
			}
			case AuthType::Bearer:
				return QByteArrayLiteral("Bearer ") + token.toUtf8();
			case AuthType::OAuth2:
				// token is populated at runtime by the async OAuth2 token fetch (M2);
				// until then it is empty and the header is simply "Bearer ".
				return QByteArrayLiteral("Bearer ") + token.toUtf8();
			default:
				return {};
			}
		}

		/// Header name to use for API Key auth (Header placement).
		QByteArray apiKeyHeaderName() const { return apiKey.toUtf8(); }

		/// Header value to use for API Key auth (Header placement).
		QByteArray apiKeyHeaderValue() const { return apiValue.toUtf8(); }
	};
}

#pragma pack(pop)
