/*
@Brief:			Qt multi-threaded network request module – proxy configuration

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

#include <QNetworkProxy>

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
	// 代理配置
	struct ProxyConfig
	{
		bool enabled{ false };
		QNetworkProxy::ProxyType type{ QNetworkProxy::HttpProxy };
		QString host;
		quint16 port{ 1080 };
		QString user;
		QString password;

		QNetworkProxy toQNetworkProxy() const
		{
			if (user.isEmpty())
				return QNetworkProxy(type, host, port);
			return QNetworkProxy(type, host, port, user, password);
		}
	};
}

#pragma pack(pop)
