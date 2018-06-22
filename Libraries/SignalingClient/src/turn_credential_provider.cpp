#include "turn_credential_provider.h"

namespace
{
	// null deleter to conform rtc::Thread* to std::shared_ptr interface safely
	struct NullDeleter { template<typename T> void operator()(T*) {} };
}

TurnCredentialProvider::TurnCredentialProvider(const std::string& uri) :
	TurnCredentialProvider(uri, std::make_shared<SslCapableSocket::Factory>())
{
}

TurnCredentialProvider::TurnCredentialProvider(const std::string& uri, std::shared_ptr<SslCapableSocket::Factory> async_socket_factory) :
	state_(State::NOT_ACTIVE),
	async_socket_factory_(async_socket_factory)
{
	// take the hostname, <protocol>://<hostname>[:port]/ 
	auto tempAuthHost = uri.substr(uri.find_first_of("://") + 3);
	auto firstSlash = tempAuthHost.find_first_of("/");
	std::string tempFragment;
	if (firstSlash != tempAuthHost.npos)
	{
		tempFragment = tempAuthHost.substr(firstSlash);
	}

	tempAuthHost = tempAuthHost.substr(0, firstSlash);
	tempAuthHost = tempAuthHost.substr(0, tempAuthHost.find_first_of(":"));

	// take the /path?whaterver uri fragment
	fragment_ = tempFragment;

	auto authorityPort = std::string("https://").compare(uri.substr(0, 8)) == 0 ? 443 : 80;
	host_ = rtc::SocketAddress(tempAuthHost, authorityPort);

	// configure the thread which will be used for socket signalling. it's just some representation of
	// the current thread (wrapped or existing)
	auto socketThread = rtc::Thread::Current();
	socketThread = socketThread == nullptr ? rtc::ThreadManager::Instance()->WrapCurrentThread() : socketThread;
	signaling_thread_ = std::shared_ptr<rtc::Thread>(socketThread, NullDeleter());
	
	socket_ = async_socket_factory_->Allocate(host_.family(), authorityPort == 443, signaling_thread_);

	socket_->SignalConnectEvent.connect(this, &TurnCredentialProvider::SocketOpen);
	socket_->SignalReadEvent.connect(this, &TurnCredentialProvider::SocketRead);
	socket_->SignalCloseEvent.connect(this, &TurnCredentialProvider::SocketClose);
}

TurnCredentialProvider::~TurnCredentialProvider()
{
	if (auth_provider_ != nullptr)
	{
		auth_provider_->SignalAuthenticationComplete.disconnect(this);
	}
}

void TurnCredentialProvider::SetAuthenticationProvider(AuthenticationProvider* authProvider)
{
	auth_provider_ = authProvider;
	auth_provider_->SignalAuthenticationComplete.connect(this, &TurnCredentialProvider::OnAuthenticationComplete);
}

bool TurnCredentialProvider::RequestCredentials()
{
	if (state_ != State::NOT_ACTIVE)
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

	// if we need to resolve the ip we do that before connecting
	if (host_.IsUnresolvedIP())
	{
		state_ = RESOLVING;
		resolver_.reset(new rtc::AsyncResolver());
		resolver_->SignalDone.connect(this, &TurnCredentialProvider::AddressResolve);
		resolver_->Start(host_);

		return true;
	}
	else if (auth_token_.empty())
	{
		state_ = AUTHENTICATING;
		return auth_provider_->Authenticate();
	}
	else
	{
		// connect the socket 
		int err = socket_->Connect(host_);
		return err != SOCKET_ERROR;
	}
}

const TurnCredentialProvider::State& TurnCredentialProvider::state() const
{
	return state_;
}


void TurnCredentialProvider::SocketOpen(rtc::AsyncSocket* socket)
{
	if (state_ != State::NOT_ACTIVE)
	{
		return;
	}

	// format the request
	std::string data = "GET " + fragment_ + " HTTP/1.1\r\n"
		"Host: " + host_.hostname() + "\r\n"
		"Authorization: Bearer " + auth_token_ + "\r\n"
		"\r\n";

	// send it 
	auto sent = socket_->Send(data.c_str(), data.length());
	RTC_DCHECK(sent == data.length());

	state_ = State::ACTIVE;
}

void TurnCredentialProvider::SocketRead(rtc::AsyncSocket* socket)
{
	if (state_ == State::ACTIVE)
	{
		TurnCredentials completionData;
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
		
		size_t bodyStart = data.find("\r\n\r\n");

		// if we didn't have a body, something is up.
		// we want to ignore that data
		if (data.empty() ||
			bodyStart == std::string::npos)
		{
			return;
		}

		Json::Reader reader;
		Json::Value root = NULL;

		// read the response 
		if (reader.parse(data.substr(bodyStart), root, true))
		{
			auto usernameWrapper = root.get("username", NULL);
			auto passwordWrapper = root.get("password", NULL);
			if (usernameWrapper != NULL && passwordWrapper != NULL)
			{
				auto username = usernameWrapper.asString();
				auto password = passwordWrapper.asString();

				// if we have values, success
				if (!username.empty() && !password.empty())
				{
					completionData.successFlag = true;
					completionData.username = username;
					completionData.password = password;
				}
			}
		}

		// emit the event
		SignalCredentialsRetrieved.emit(completionData);

		// after emission, we can close our socket 
		// note: we don't really mind if this fails, it doesn't matter until 
		// we try another request, at which point we'll handle the error on that call 
		socket_->Close();

		// after emission, we're totally done, and move to a NOT_ACTIVE state to allow 
		// more authentication requests to be triggered 
		state_ = State::NOT_ACTIVE;
	}
}

void TurnCredentialProvider::SocketClose(rtc::AsyncSocket* socket, int err)
{
	state_ = State::NOT_ACTIVE;
}

void TurnCredentialProvider::AddressResolve(rtc::AsyncResolverInterface* resolver)
{
	// capture the result
	auto tempHost = resolver->address();

	// release the resolver pointer and destroy the resolver
	auto released = resolver_.release();
	released->Destroy(true);

	// reset the resolver pointer to nullptr
	resolver_.reset(nullptr);

	if (state_ != State::RESOLVING)
	{
		return;
	}

	host_ = tempHost;

	if (auth_token_.empty())
	{
		state_ = AUTHENTICATING;
		if (!auth_provider_->Authenticate())
		{
			return;
		}
	}
	else
	{
		state_ = State::NOT_ACTIVE;
	
		// connect the socket 
		int err = socket_->Connect(host_);
	}
}

void TurnCredentialProvider::OnAuthenticationComplete(const AuthenticationProviderResult& result)
{
	if (state_ != State::AUTHENTICATING)
	{
		return;
	}

	if (!result.successFlag)
	{
		return;
	}

	auth_token_ = result.accessToken;

	state_ = State::NOT_ACTIVE;

	// connect the socket 
	int err = socket_->Connect(host_);
}