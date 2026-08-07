#include "networkrequestregistry.h"
#include "networkrequest.h"
#include <algorithm>

namespace QtNetworkRequest
{

NetworkRequestRegistry& NetworkRequestRegistry::instance()
{
	// Meyer's Singleton — guaranteed thread-safe initialization (C++11)
	static NetworkRequestRegistry s_instance;
	return s_instance;
}

void NetworkRequestRegistry::registerCreator(RequestType type, int priority, Creator creator)
{
	m_registry[type].append({priority, std::move(creator)});
	// Keep list sorted by priority descending for deterministic priority-based dispatch
	std::sort(m_registry[type].begin(), m_registry[type].end(),
		[](const QPair<int, Creator>& a, const QPair<int, Creator>& b) {
			return a.first > b.first;
		});
}

std::unique_ptr<NetworkRequest> NetworkRequestRegistry::create(std::unique_ptr<RequestContext> context)
{
	if (!context)
		return nullptr;

	auto it = m_registry.find(context->type);
	if (it == m_registry.end() || it->isEmpty())
		return nullptr;

	// Try creators in priority order; first non-null result wins.
	// Creators receive a raw pointer (non-owning), so conditional routing
	// (e.g. Download -> MTDownload vs single-thread) can inspect the context
	// without consuming it.
	for (const auto& pair : it.value())
	{
		NetworkRequest* raw = pair.second(context.get());
		if (raw)
		{
			// Transfer context ownership into the created request
			raw->setRequestContext(std::move(context));
			return std::unique_ptr<NetworkRequest>(raw);
		}
	}

	return nullptr;
}

} // namespace QtNetworkRequest
