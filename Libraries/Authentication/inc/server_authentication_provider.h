#pragma once

#include <vector>
#include <string>

#include "webrtc/rtc_base/sigslot.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/physicalsocketserver.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "authentication_provider.h"
#include "ssl_capable_socket.h"

class ServerAuthenticationProvider : public sigslot::has_slots<>,
	public AuthenticationProvider
{
public:
	struct ServerAuthInfo
	{
	public:
		std::string clientId;
		std::string clientSecret;
		std::string authority;
		std::string resource;
	};

	enum State
	{
		NOT_ACTIVE = 0,
		RESOLVING,
		ACTIVE
	};

	ServerAuthenticationProvider(const ServerAuthInfo& authInfo);

	const State& state() const;

	// implement AuthenticationProvider
	virtual bool Authenticate() override;

protected:
	void SocketOpen(rtc::AsyncSocket* socket);
	void SocketRead(rtc::AsyncSocket* socket);
	void SocketClose(rtc::AsyncSocket* socket, int err);
	void AddressResolve(rtc::AsyncResolverInterface* resolver);

	ServerAuthInfo auth_info_;
	rtc::SocketAddress authority_host_;
	State state_;
	std::unique_ptr<SslCapableSocket> socket_;
	std::unique_ptr<AsyncResolver> resolver_;
};