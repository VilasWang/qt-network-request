/*
@Brief:			Qt multi-threaded network request module – environment variable substitution

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

#include "environment.h"

#include <QRegularExpression>

namespace QtNetworkRequest
{
	QString substituteEnv(const QString &text, const QMap<QString, QString> &vars)
	{
		if (vars.isEmpty() || text.isEmpty())
			return text;

	// Match {{key}} where key is [\w.-]+ (no inner whitespace). Unknown keys
	// are left intact by returning the original captured text (Postman-style).
	// Implemented with a globalMatch iterator (instead of the Qt5.15-only
	// replace(QRegularExpression, lambda) overload) for portability.
	static const QRegularExpression re(R"(\{\{(\w[\w.-]*)\}\})");
	QString result;
	result.reserve(text.size());
	int lastPos = 0;
	QRegularExpressionMatchIterator it = re.globalMatch(text);
	while (it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		const int start = m.capturedStart();
		const int end = m.capturedEnd();
		if (start > lastPos)
			result.append(text.mid(lastPos, start - lastPos));
		const QString key = m.captured(1);
		const auto vit = vars.constFind(key);
		result.append((vit != vars.constEnd()) ? vit.value() : m.captured(0));
		lastPos = end;
	}
	if (lastPos < text.size())
		result.append(text.mid(lastPos));
	return result;
	}

	void applyEnvironment(RequestContext &ctx, const QMap<QString, QString> &vars)
	{
		if (vars.isEmpty())
			return;

		ctx.url = substituteEnv(ctx.url, vars);

		for (auto it = ctx.headers.begin(); it != ctx.headers.end(); ++it)
			it.value() = substituteEnv(QString::fromUtf8(it.value()), vars).toUtf8();

		// Note: body substitution matches Postman behaviour — {{var}} placeholders
		// in request bodies are resolved. If a literal {{...}} value is needed,
		// escape it by doubling the braces (e.g. {{{{key}}}}).
		ctx.body = substituteEnv(ctx.body, vars);

		for (auto it = ctx.queryParams.begin(); it != ctx.queryParams.end(); ++it)
			it.value() = substituteEnv(it.value(), vars);

		// (R3) AuthConfig fields live outside url/headers/body/queryParams, so a
		// tokenUrl like "{{host}}/oauth/token" or an apiKey value "{{apiKey}}"
		// would otherwise be sent verbatim. Substitute the relevant fields.
		AuthConfig &a = ctx.authConfig;
		switch (a.type)
		{
		case AuthType::Basic:
			a.username = substituteEnv(a.username, vars);
			a.password = substituteEnv(a.password, vars);
			break;
		case AuthType::Bearer:
			a.token = substituteEnv(a.token, vars);
			break;
		case AuthType::ApiKey:
			a.apiKey   = substituteEnv(a.apiKey, vars);
			a.apiValue = substituteEnv(a.apiValue, vars);
			break;
		case AuthType::OAuth2:
			a.oauth2Config.tokenUrl     = substituteEnv(a.oauth2Config.tokenUrl, vars);
			a.oauth2Config.clientId     = substituteEnv(a.oauth2Config.clientId, vars);
			a.oauth2Config.clientSecret = substituteEnv(a.oauth2Config.clientSecret, vars);
			a.oauth2Config.scopes       = substituteEnv(a.oauth2Config.scopes, vars);
			a.oauth2Config.username     = substituteEnv(a.oauth2Config.username, vars);
			a.oauth2Config.password     = substituteEnv(a.oauth2Config.password, vars);
			a.oauth2Config.refreshToken = substituteEnv(a.oauth2Config.refreshToken, vars);
			break;
		default:
			break;
		}

		// binaryBody intentionally NOT mutated.
	}
}
