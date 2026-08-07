#include "progressthrottle.h"

namespace QtNetworkRequest
{

ProgressThrottle::ProgressThrottle(int intervalMs, QObject* parent)
	: QObject(parent), m_intervalMs(intervalMs)
{
	m_timer.setInterval(m_intervalMs);
	connect(&m_timer, &QTimer::timeout, this, [this]() {
		m_ready = true;
		emit gateOpened();
	});
}

void ProgressThrottle::start()
{
	m_timer.start();
}

void ProgressThrottle::stop()
{
	m_timer.stop();
}

void ProgressThrottle::report(qint64 bytesSent, qint64 bytesTotal,
							   const std::function<void(qint64, qint64)>& emitter)
{
	if (!m_ready || bytesSent <= 0 || bytesTotal <= 0)
		return;

	m_ready = false;  // close gate until next timer tick

	if (emitter)
		emitter(bytesSent, bytesTotal);
}

} // namespace QtNetworkRequest
