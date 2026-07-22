/*
@Brief:			Qt multi-threaded network request module – SSL/TLS configuration

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

#ifndef QT_NO_SSL
#include <QSslCertificate>
#include <QSslError>
#endif

#pragma pack(push, _CRT_PACKING)

namespace QtNetworkRequest
{
#ifndef QT_NO_SSL
	// SSL/TLS 安全策略配置
	// 两级配置：全局默认 (NetworkRequestManager::setGlobalSslConfig) + 每请求覆盖 (RequestContext::sslConfig)。
	// 每请求字段为 Inherit 时继承全局对应值；全局字段经 setGlobalSslConfig 规范化后永不为 Inherit。
	struct SslConfig
	{
		// 证书对端验证模式
		enum class PeerVerifyMode : int8_t
		{
			Inherit    = -1,  // per-request: 继承全局默认
			VerifyNone = 0,   // 不验证（不安全，仅内网/测试）
			VerifyPeer = 1,   // 验证对端证书（安全默认）
		};
		// TLS 最低协议版本
		enum class TlsProtocol : int8_t
		{
			Inherit     = -1,
			TlsV1_0     = 0,
			TlsV1_1     = 1,
			TlsV1_2     = 2,  // 安全默认
			TlsV1_3     = 3,  // Qt 5.12+
			AnyProtocol = 4,  // 由 Qt/系统决定
		};
		// SSL 错误忽略策略
		enum class IgnorePolicy : int8_t
		{
			Inherit              = -1,  // per-request: 继承全局
			Never                = 0,   // 不忽略任何错误（安全默认）
			IgnoreSpecificErrors = 1,   // 仅忽略 ignoreErrorTypes 中列出的错误类型
			Always               = 2,   // 忽略所有 sslErrors（NOT FOR PRODUCTION，需审计日志）
		};
		// CA 证书策略
		enum class CaPolicy : int8_t
		{
			Inherit       = -1,  // per-request: 继承全局
			SystemDefault = 0,   // 使用系统默认 CA
			Custom        = 1,   // 使用 caCertificates 字段（替换系统 CA）
		};

		PeerVerifyMode peerVerifyMode{ PeerVerifyMode::Inherit };
		TlsProtocol    minProtocol{ TlsProtocol::Inherit };
		IgnorePolicy   ignoreSslErrorsPolicy{ IgnorePolicy::Inherit };
		CaPolicy       caPolicy{ CaPolicy::Inherit };
		QList<QSslCertificate> caCertificates;        // 仅 CaPolicy::Custom 时使用
		QList<QSslError::SslError> ignoreErrorTypes;  // 仅 IgnorePolicy::IgnoreSpecificErrors 时使用

		// 安全默认配置（用于全局默认初始化）
		static SslConfig secureDefault()
		{
			SslConfig c;
			c.peerVerifyMode        = PeerVerifyMode::VerifyPeer;
			c.minProtocol           = TlsProtocol::TlsV1_2;
			c.ignoreSslErrorsPolicy = IgnorePolicy::Never;
			c.caPolicy              = CaPolicy::SystemDefault;
			return c;
		}
	};
#endif
}

#pragma pack(pop)
