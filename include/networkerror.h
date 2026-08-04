/*
@Brief:			Qt multi-threaded network request module – structured error

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

#include <cstdint>
#include <QString>

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
	// 错误分类 (Qt 无关)
	enum class ErrorCategory : int32_t
	{
		None = 0,       // 无错误 (成功)
		Cancelled,      // 用户取消
		Timeout,        // 超时
		Network,        // 网络传输层错误
		Ssl,            // SSL/TLS 错误
		Http,           // HTTP 状态码错误 (>=400)
		FileIo,         // 本地文件/IO 错误
		Protocol,       // 协议层错误
		Configuration,  // 请求配置错误
		Unknown,        // 未分类错误
	};

	// 稳定的库级错误码 (与 Qt 解耦)
	enum class ErrorCode : int32_t
	{
		NoError = 0,

		// 取消
		OperationCancelled,

		// 超时
		TimeoutTotal,
		TimeoutIdle,
		TimeoutTransfer,

		// 网络
		ConnectionRefused,
		HostNotFound,
		RemoteHostClosed,
		TemporaryNetworkFailure,
		ProxyError,
		TooManyRedirects,

		// SSL
		SslHandshakeFailed,

		// HTTP
		HttpClientError,   // 4xx
		HttpServerError,   // 5xx

		// 协议 / 配置
		InvalidUrl,
		UnsupportedProtocol,
		UnsupportedRequestType,
		InvalidReply,
		ContentLengthMissing,

		// 认证
		AuthInvalid,          // Invalid or missing authentication credentials
		AuthTypeUnsupported,  // Authentication type recognized but not yet implemented

		// 文件 / IO
		FileOpenFailed,
		FileWriteFailed,
		FileReadFailed,
		MemoryMappingFailed,
		FileRenameFailed,

		// 兜底
		TerminatedWithoutResponse,
		Unknown,
	};

	// 结构化错误信息
	struct ErrorInfo
	{
		ErrorCategory category{ ErrorCategory::None };
		ErrorCode code{ ErrorCode::NoError };
		int nativeCode{ 0 };   // QNetworkReply::NetworkError, 仅用于诊断
		QString message;

		bool isError() const { return category != ErrorCategory::None; }
	};

	// 由 HTTP 状态码构造结构化错误 (>=500 -> HttpServerError, 400..499 -> HttpClientError)
	ErrorInfo makeHttpError(int statusCode, const QString& message = QString());

	// 调试/日志辅助
	const char* toString(ErrorCategory category);
	const char* toString(ErrorCode code);
}

#pragma pack(pop)
