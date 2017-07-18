#include "pch.h"

#include "server_authentication_provider.h"

ServerAuthenticationProvider::ServerAuthenticationProvider(const ServerAuthInfo& authInfo) : AuthenticationProvider(), auth_info_(authInfo)
{
	if (authInfo.authority.empty() || authInfo.clientId.empty() || authInfo.clientSecret.empty())
	{
		LOG(LS_ERROR) << __FUNCTION__ << ": invalid authInfo, empty";
		throw std::exception("invalid authInfo, empty");
	}

	auto authority = authInfo.authority;

	// take the hostname, <protocol>://<hostname>[:port]/ 
	auto tempAuthHost = authority.substr(authority.find_first_of("://") + 3);
	tempAuthHost = tempAuthHost.substr(0, tempAuthHost.find_first_of("/"));
	tempAuthHost = tempAuthHost.substr(0, tempAuthHost.find_first_of(":"));

	auto authorityPort = std::string("https://").compare(authority.substr(0, 8)) == 0 ? 443 : 80;
	authority_host_ = rtc::SocketAddress(tempAuthHost, authorityPort);

	auto socketThread = rtc::Thread::Current();
	socketThread = socketThread == nullptr ? rtc::ThreadManager::Instance()->WrapCurrentThread() : socketThread;
	socket_.reset(new SslCapableSocket(authority_host_.family(), authorityPort == 443, socketThread));

	socket_->SignalConnectEvent.connect(this, &ServerAuthenticationProvider::SocketOpen);
	socket_->SignalReadEvent.connect(this, &ServerAuthenticationProvider::SocketRead);
	socket_->SignalCloseEvent.connect(this, &ServerAuthenticationProvider::SocketClose);
}

const ServerAuthenticationProvider::State& ServerAuthenticationProvider::state() const
{
	return state_;
}

bool ServerAuthenticationProvider::Authenticate()
{
	if (state_ != ServerAuthenticationProvider::State::NOT_ACTIVE)
	{
		return false;
	}

	if (socket_->GetState() != rtc::Socket::CS_CLOSED)
	{
		if (0 != socket_->Close())
		{
			return false;
		}
	}

	if (authority_host_.IsUnresolvedIP())
	{
		state_ = RESOLVING;
		resolver_.reset(new rtc::AsyncResolver());
		resolver_->SignalDone.connect(this, &ServerAuthenticationProvider::AddressResolve);
		resolver_->Start(authority_host_);

		return true;
	}
	else
	{
		// connect the socket 
		int err = socket_->Connect(authority_host_);

		return err != SOCKET_ERROR;
	}
}

void ServerAuthenticationProvider::SocketOpen(rtc::AsyncSocket* socket)
{
	if (state_ != State::NOT_ACTIVE)
	{
		return;
	}

	// format the request 
	std::string dataBody = "grant_type=client_credentials&"
		"client_id=" + auth_info_.clientId + "&"
		"client_secret=" + auth_info_.clientSecret + "&"
		"resource=" + auth_info_.resource;

	std::string data = "POST " + auth_info_.authority + " HTTP/1.1\r\n"
		"Host: " + authority_host_.hostname() + "\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n" +
		"Content-Length: " + std::to_string(dataBody.length()) + "\r\n\n"
		+ dataBody +
		"\r\n";

	// send it 
	auto sent = socket_->Send(data.c_str(), data.length());
	RTC_DCHECK(sent == data.length());

	state_ = State::ACTIVE;
}

void ServerAuthenticationProvider::SocketRead(rtc::AsyncSocket* socket)
{
	if (state_ == State::ACTIVE)
	{
		AuthenticationProviderResult completionData;
		completionData.successFlag = false;

		// parse response 
		std::string data;
		char buffer[0xffff];
		do
		{
			int bytes = socket_->Recv(buffer, sizeof(buffer), nullptr);
			if (bytes <= 0)
			{
				break;
			}

			data.append(buffer, bytes);
		} while (true);

		// sometimes we get this event but the
		// data is empty. we want to ignore that
		if (data.empty())
		{
			return;
		}

		size_t bodyStart = data.find("\r\n\r\n");

		Json::Reader reader;
		Json::Value root = NULL;

		// read the response 
		if (reader.parse(data.substr(bodyStart), root, true))
		{
			auto tokenWrapper = root.get("access_token", NULL);
			if (tokenWrapper != NULL)
			{
				auto token = tokenWrapper.asString();

				// if we have a token, success 
				if (!token.empty())
				{
					completionData.successFlag = true;
					completionData.accessToken = token;
				}
			}
		}

		// emit the event
		if (callback_ != nullptr)
		{
			callback_->OnAuthenticationComplete(completionData);
		}

		// after emission, we can close our socket 
		// note: we don't really mind if this fails, it doesn't matter until 
		// we try another request, at which point we'll handle the error on that call 
		socket_->Close();

		// after emission, we're totally done, and move to a NOT_ACTIVE state to allow 
		// more authentication requests to be triggered 
		state_ = State::NOT_ACTIVE;
	}
}

void ServerAuthenticationProvider::SocketClose(rtc::AsyncSocket* socket, int err)
{
	state_ = State::NOT_ACTIVE;
}

void ServerAuthenticationProvider::AddressResolve(rtc::AsyncResolverInterface* resolver)
{
	auto tempHost = resolver->address();
	auto released = resolver_.release();
	released->Destroy(true);
	resolver_.reset(nullptr);

	if (state_ != State::RESOLVING)
	{
		return;
	}

	state_ = State::NOT_ACTIVE;
	authority_host_ = tempHost;

	// connect the socket 
	int err = socket_->Connect(authority_host_);
}