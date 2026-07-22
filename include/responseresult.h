/*
@Brief:			Qt multi-threaded network request module – response result

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

#include <QMap>
#include <QByteArray>
#include <QVariant>
#include <QSharedPointer>
#include "taskdata.h"

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
	// 响应结果 (Output)
	struct ResponseResult
	{
		bool success{ false };
		bool cancelled{ false };
		bool timeout{ false };  // true if request terminated due to timeout (not cancellation)
		int statusCode{ 0 };
		int errorCode{ 0 };     // QNetworkReply::NetworkError when failed
		QString errorMessage;
		QByteArray body;
		QMap<QByteArray, QByteArray> headers;

		TaskData task;

		// 用户自定义上下文
		QVariant userContext;

		// 性能统计
		struct Performance
		{
			quint64 durationMs{ 0 };
			qint64 bytesReceived{ 0 };
			qint64 bytesSent{ 0 };
		} performance;
	};
}

Q_DECLARE_METATYPE(QSharedPointer<QtNetworkRequest::ResponseResult>);

#pragma pack(pop)
