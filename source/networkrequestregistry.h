#pragma once

#include <functional>
#include <memory>
#include <QMap>
#include <QList>
#include <QPair>
#include "requestcontext.h"

namespace QtNetworkRequest
{

class NetworkRequest;

/// @brief Self-registering factory for NetworkRequest subclasses.
///
/// Replaces the hard-coded switch-case in NetworkRequestFactory::create().
/// Each request type registers itself via a static initializer in its .cpp file,
/// so adding a new request type never requires modifying core code.
///
/// Usage (in each subclass .cpp):
/// @code
///   static const int _reg = []() -> int {
///       NetworkRequestRegistry::instance().registerCreator(
///           RequestType::Get, 10,
///           [](RequestContext* ctx) { return new MyRequest(ctx); });
///       return 0;
///   }();
/// @endcode
///
/// Note: Creators receive a non-owning raw pointer to allow conditional routing
/// (multiple creators can inspect the context; only the winner claims ownership).
class NetworkRequestRegistry
{
public:
	/// Takes non-owning RequestContext*. Returns owned NetworkRequest* or nullptr.
	using Creator = std::function<NetworkRequest*(RequestContext*)>;

	static NetworkRequestRegistry& instance();

	/// Register a creator for a request type.
	/// @param type      Target request type
	/// @param priority  Higher = checked first.  Default 0.
	/// @param creator   Factory function; return nullptr to skip (allows conditional routing)
	void registerCreator(RequestType type, int priority, Creator creator);

	/// Create a request for the given context. Returns nullptr if no creator matches.
	/// Transfers ownership of the context to the created request.
	std::unique_ptr<NetworkRequest> create(std::unique_ptr<RequestContext> context);

private:
	NetworkRequestRegistry() = default;

	// type -> [(priority, creator)] sorted by priority descending
	QMap<RequestType, QList<QPair<int, Creator>>> m_registry;
};

} // namespace QtNetworkRequest
