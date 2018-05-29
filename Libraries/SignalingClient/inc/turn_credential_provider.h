#pragma once
#include "webrtc/rtc_base/sigslot.h"
#include "webrtc/rtc_base/nethelpers.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "authentication_provider.h"
#include "ssl_capable_socket.h"

// forward decl
class TurnCredentialProvider;

struct TurnCredentials
{
public:
	bool successFlag;
	std::string username;
	std::string password;
private:
	friend TurnCredentialProvider;
	TurnCredentials() {};
};

class TurnCredentialProvider : public sigslot::has_slots<>
{
public:
	enum State
	{
		NOT_ACTIVE = 0,
		RESOLVING,
		AUTHENTICATING,
		ACTIVE
	};

	TurnCredentialProvider(const std::string& uri);
	~TurnCredentialProvider();

	sigslot::signal1<const TurnCredentials&> SignalCredentialsRetrieved;
	
	struct CredentialsRetrievedCallback : public sigslot::has_slots<>
	{
		CredentialsRetrievedCallback(const std::function<void(const TurnCredentials&)>& handler) : handler_(handler)
		{
		}

		void Handle(const TurnCredentials& data)
		{
			handler_(data);
		}
	private:
		std::function<void(const TurnCredentials&)> handler_;
	};

	void SetAuthenticationProvider(AuthenticationProvider* authProvider);

	bool RequestCredentials();
	
	const State& state() const;

	void OnAuthenticationComplete(const AuthenticationProviderResult& result);

protected:
	void SocketOpen(rtc::AsyncSocket* socket);
	void SocketRead(rtc::AsyncSocket* socket);
	void SocketClose(rtc::AsyncSocket* socket, int err);
	void AddressResolve(rtc::AsyncResolverInterface* resolver);

	rtc::SocketAddress host_;
	std::string fragment_;
	std::string auth_token_;
	State state_;
	std::unique_ptr<SslCapableSocket> socket_;
	std::unique_ptr<AsyncResolver> resolver_;
	AuthenticationProvider* auth_provider_;
};
