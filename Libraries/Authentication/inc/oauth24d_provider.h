#pragma once

#include <string>

#include "webrtc/rtc_base/sigslot.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/physicalsocketserver.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "ssl_capable_socket.h"
#include "authentication_provider.h"

class OAuth24DProvider : public sigslot::has_slots<>,
	public MessageHandler,
	public AuthenticationProvider
{
public:

	enum State
	{
		NOT_ACTIVE = 0,
		RESOLVING_CODE,
		RESOLVING_POLL,
		REQUEST_CODE,
		REQUEST_POLL,
		ACTIVE_CODE,
		ACTIVE_POLL
	};

	struct CodeData
	{
		std::string device_code;
		std::string user_code;
		int interval;
		std::string verification_url;
	};

	OAuth24DProvider(const std::string& codeUri, const std::string& pollUri);

	~OAuth24DProvider();

	const State& state() const;

	// emitted when we have the code response and are awaiting user interaction
	sigslot::signal1<const CodeData&> SignalCodeComplete;

	struct CodeCompleteCallback : public sigslot::has_slots<>
	{
		CodeCompleteCallback(const std::function<void(const CodeData&)>& handler) : handler_(handler)
		{
		}

		void Handle(const CodeData& data)
		{
			handler_(data);
		}
	private:
		std::function<void(const CodeData&)> handler_;
	};

	// implement AuthenticationProvider
	virtual bool Authenticate() override;

	// implement MessageHandler
	virtual void OnMessage(rtc::Message* msg) override;

protected:
	const int kPollMessageId = 1349U;

	void SocketOpen(rtc::AsyncSocket* socket);
	void SocketRead(rtc::AsyncSocket* socket);
	void SocketClose(rtc::AsyncSocket* socket, int err);
	void AddressResolve(rtc::AsyncResolverInterface* resolver);

	std::string code_uri_;
	std::string poll_uri_;
	rtc::SocketAddress code_host_;
	rtc::SocketAddress poll_host_;
	State state_;
	CodeData data_;
	std::shared_ptr<rtc::Thread> signaling_thread_;
	std::unique_ptr<SslCapableSocket> socket_;
	std::unique_ptr<AsyncResolver> resolver_;

private:
	rtc::SocketAddress SocketAddressFromString(const std::string& str);
	int ConnectSocket(rtc::SocketAddress addr);
};
