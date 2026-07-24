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
#include "networkerror.h"

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
	// 响应结果 (Output)
	struct ResponseResult
	{
		// 结构化错误 (error.isError()==false 即成功)
		ErrorInfo error;
		int statusCode{ 0 };   // HTTP 状态码 (成功时也有意义)
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

		// 便捷只读访问器 (纯派生, 不存储冗余状态)
		bool isSuccess() const { return !error.isError(); }
		bool isCancelled() const { return error.category == ErrorCategory::Cancelled; }
		bool isTimeout() const { return error.category == ErrorCategory::Timeout; }
	};
}

Q_DECLARE_METATYPE(QSharedPointer<QtNetworkRequest::ResponseResult>);

#pragma pack(pop)
