#pragma once

#include <QObject>
#include <QTimer>
#include <functional>

namespace QtNetworkRequest
{

/// @brief Thread-safe progress throttle — fires at most once per interval.
///
/// Replaces the duplicated throttle pattern in NetworkDownloadRequest,
/// NetworkUploadRequest, and Downloader. The throttle is driven by a
/// QTimer; the consumer connects to throttled() or passes an emitter lambda
/// to report().
class ProgressThrottle : public QObject
{
	Q_OBJECT

public:
	explicit ProgressThrottle(int intervalMs = 250, QObject* parent = nullptr);

	/// Start the internal timer. Call after constructing / reusing.
	void start();

	/// Stop the internal timer.
	void stop();

	/// Report progress. If the gate is open, the emitter is invoked and
	/// the gate is closed until the next timer tick. Thread-safe: the
	/// emitter is callable from any thread since it typically posts an event.
	/// @param bytesSent   bytes sent / received so far
	/// @param bytesTotal  total bytes expected
	/// @param emitter     invoked when the gate is open (and progress > 0)
	void report(qint64 bytesSent, qint64 bytesTotal,
				const std::function<void(qint64, qint64)>& emitter);

	/// Returns true if the gate is currently open (ready to emit).
	bool isReady() const { return m_ready; }

Q_SIGNALS:
	/// Emitted when the gate opens on timer tick.
	void gateOpened();

private:
	QTimer m_timer;
	int    m_intervalMs;
	bool   m_ready = false;  // gate: true when ready to emit next progress
};

} // namespace QtNetworkRequest
