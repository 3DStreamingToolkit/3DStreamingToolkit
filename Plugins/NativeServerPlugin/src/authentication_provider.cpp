#include "authentication_provider.h"

AuthenticationProvider::AuthenticationProvider(const std::string& clientId, const std::string& authority) :
	AuthenticationProvider(clientId, authority, std::vector<std::string>())
{
	socket_->SignalConnectEvent.connect(this, &AuthenticationProvider::SocketOpen);
	socket_->SignalReadEvent.connect(this, &AuthenticationProvider::SocketRead);
	socket_->SignalWriteEvent.connect(this, &AuthenticationProvider::SocketWrite);
	socket_->SignalCloseEvent.connect(this, &AuthenticationProvider::SocketClose);
}

AuthenticationProvider::AuthenticationProvider(const std::string& clientId, const std::string& authority, const std::vector<std::string>& b2cScopes) :
	client_id_(clientId), authority_(authority), b2c_scopes_(b2cScopes)
{
}

const AuthenticationProvider::State& AuthenticationProvider::state() const
{
	return state_;
}

void AuthenticationProvider::Authenticate(const std::string& clientId, const std::string& clientSecret)
{
	if (state_ != AuthenticationProvider::State::NOT_ACTIVE)
	{
		return;
	}

	RTC_DCHECK(socket_->GetState() == rtc::Socket::CS_CLOSED);

	std::string data = "";
	auto sent = socket_->Send(data.c_str(), data.length());
	RTC_DCHECK(sent == data.length());
}

void AuthenticationProvider::SocketOpen(rtc::AsyncSocket* socket)
{

}

void AuthenticationProvider::SocketRead(rtc::AsyncSocket* socket)
{

}

void AuthenticationProvider::SocketWrite(rtc::AsyncSocket* socket)
{

}

void AuthenticationProvider::SocketClose(rtc::AsyncSocket* socket, int err)
{

}