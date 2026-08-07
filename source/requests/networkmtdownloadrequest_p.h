#pragma once

#include <QObject>
#include <QNetworkReply>
#include <memory>

namespace QtNetworkRequest
{

class NetworkMTDownloadRequest;

/// @brief State machine state for the multi-threaded download lifecycle.
///
/// Replaces the implicit phase switching (HEAD → Range probe → multi-download)
/// with explicit state objects. Each state encapsulates:
///   - Phase entry (enter): sends the network request
///   - Phase completion (onFinished): processes the response
///   - Phase error (onError): handles network errors
class IMDTDownloadState
{
public:
	virtual ~IMDTDownloadState() = default;

	/// Called when this state becomes active. Sends the phase-specific request.
	virtual void enter(NetworkMTDownloadRequest* ctx) = 0;

	/// Called when m_networkReply emits finished().
	virtual void onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply) = 0;

	/// Called when m_networkReply emits an error signal.
	virtual void onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code) = 0;

	/// Human-readable name for logging / debugging.
	virtual QString name() const = 0;
};

// ============================================================================
// ProbeState — sends HEAD request to get Content-Length
// ============================================================================
class ProbeState : public IMDTDownloadState
{
public:
	void enter(NetworkMTDownloadRequest* ctx) override;
	void onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply) override;
	void onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code) override;
	QString name() const override { return QStringLiteral("Probe"); }
};

// ============================================================================
// RangeProbeState — sends GET with Range: bytes=0-0 to test Range support
// ============================================================================
class RangeProbeState : public IMDTDownloadState
{
public:
	void enter(NetworkMTDownloadRequest* ctx) override;
	void onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply) override;
	void onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code) override;
	QString name() const override { return QStringLiteral("RangeProbe"); }
};

// ============================================================================
// MultiDownloadState — creates Downloaders, monitors subpart completion
// ============================================================================
class MultiDownloadState : public IMDTDownloadState
{
public:
	void enter(NetworkMTDownloadRequest* ctx) override;
	void onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply) override;
	void onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code) override;
	QString name() const override { return QStringLiteral("MultiDownload"); }
};

} // namespace QtNetworkRequest
