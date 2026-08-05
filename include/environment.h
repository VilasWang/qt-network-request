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

#pragma once

#include <QString>
#include <QMap>
#include "requestcontext.h"          // RequestContext (authConfig, headers, ...)
#include "authconfig.h"
#include "networkrequestglobal.h"

namespace QtNetworkRequest
{
	/// Replace all `{{key}}` occurrences in `text` using `vars`.
	/// Unknown keys are left intact (Postman-style, so misconfiguration is visible).
	/// A single pass is performed — values are NOT recursively substituted.
	NETWORK_EXPORT QString substituteEnv(const QString &text, const QMap<QString, QString> &vars);

	/// Substitute `{{var}}` across a whole RequestContext: url, header values, body,
	/// query params, and the relevant AuthConfig fields (R3). `binaryBody` is never
	/// touched, because env substitution must not corrupt binary payloads.
	NETWORK_EXPORT void applyEnvironment(RequestContext &ctx, const QMap<QString, QString> &vars);
}
