#pragma once

#include <vector>
#include <string>

#include "webrtc/base/sigslot.h"
#include "webrtc/base/physicalsocketserver.h"

#include "ssl_capable_socket.h"

class AuthenticationProvider : public sigslot::has_slots<>
{
public:
	AuthenticationProvider(const std::string& clientId, const std::string& authority);
	AuthenticationProvider(const std::string& clientId, const std::string& authority, const std::vector<std::string>& b2cScopes);

	enum State
	{
		NOT_ACTIVE = 0,
		SUCCEEDED,
		FAILED
	};

	const State& state() const;

	// SignalCompleteEvent uses multi_threaded_local to allow
	// access concurrently from different thread.
	sigslot::signal1<AuthenticationProvider*,
		sigslot::multi_threaded_local> SignalCompleteEvent;

	void Authenticate(const std::string& clientId, const std::string& clientSecret);

protected:
	void SocketOpen(rtc::AsyncSocket* socket);
	void SocketRead(rtc::AsyncSocket* socket);
	void SocketWrite(rtc::AsyncSocket* socket);
	void SocketClose(rtc::AsyncSocket* socket, int err);

	std::string client_id_;
	std::string authority_;
	std::vector<std::string> b2c_scopes_;
	State state_;
	std::unique_ptr<SslCapableSocket> socket_;
};