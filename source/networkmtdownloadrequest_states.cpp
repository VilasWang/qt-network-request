#include "networkmtdownloadrequest_p.h"
#include "networkmtdownloadrequest.h"
#include "qtcompat.h"
#include <QDebug>

using namespace QtNetworkRequest;

// ============================================================================
// ProbeState — HEAD request for Content-Length
// ============================================================================

void ProbeState::enter(NetworkMTDownloadRequest* ctx)
{
	ctx->doProbeRequest();
}

void ProbeState::onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply)
{
	ctx->handleProbeFinished(reply);
}

void ProbeState::onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code)
{
	Q_UNUSED(code);
	// Errors are handled by handleProbeFinished() which checks evaluateOutcome()
}

// ============================================================================
// RangeProbeState — GET Range: bytes=0-0 to test Range support
// ============================================================================

void RangeProbeState::enter(NetworkMTDownloadRequest* ctx)
{
	ctx->doRangeProbeRequest();
}

void RangeProbeState::onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply)
{
	ctx->handleRangeProbeFinished(reply);
}

void RangeProbeState::onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code)
{
	Q_UNUSED(code);
	// Fall back to single-thread via the handle method which will be called in onFinished
}

// ============================================================================
// MultiDownloadState — creates Downloaders, monitors subpart completion
// ============================================================================

void MultiDownloadState::enter(NetworkMTDownloadRequest* ctx)
{
	ctx->doMultiDownload();
}

void MultiDownloadState::onFinished(NetworkMTDownloadRequest* ctx, QNetworkReply* reply)
{
	Q_UNUSED(ctx);
	Q_UNUSED(reply);
	// No-op: in this phase replies are handled by individual Downloaders
}

void MultiDownloadState::onError(NetworkMTDownloadRequest* ctx, QNetworkReply::NetworkError code)
{
	Q_UNUSED(ctx);
	Q_UNUSED(code);
}
