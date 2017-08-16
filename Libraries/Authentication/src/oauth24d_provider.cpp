#include "oauth24d_provider.h"

OAuth24DProvider::OAuth24DProvider(const std::string& codeUri, const std::string& pollUri) :
	code_uri_(codeUri), poll_uri_(pollUri), state_(State::NOT_ACTIVE)
{
	// don't support empty values for these fields
	if (codeUri.empty() || pollUri.empty())
	{
		LOG(LS_ERROR) << __FUNCTION__ << ": invalid parameters, empty";
		throw std::exception("invalid parameters, empty");
	}

	code_host_ = SocketAddressFromString(codeUri);
	poll_host_ = SocketAddressFromString(pollUri);

	// configure the thread which will be used for socket signalling. it's just some representation of
	// the current thread (wrapped or existing)
	auto socketThread = rtc::Thread::Current();
	socketThread = socketThread == nullptr ? rtc::ThreadManager::Instance()->WrapCurrentThread() : socketThread;

	signaling_thread_ = socketThread;
}

OAuth24DProvider::~OAuth24DProvider()
{
	if (resolver_.get() != nullptr)
	{
		// release the resolver pointer and destroy the resolver
		auto released = resolver_.release();
		released->Destroy(true);

		// reset the resolver pointer to nullptr
		resolver_.reset(nullptr);
	}
}

const OAuth24DProvider::State& OAuth24DProvider::state() const
{
	return state_;
}

rtc::SocketAddress OAuth24DProvider::SocketAddressFromString(const std::string& str)
{
	// take the hostname, <protocol>://<hostname>[:port]/ 
	auto tempHost = str.substr(str.find_first_of("://") + 3);
	tempHost = tempHost.substr(0, tempHost.find_first_of("/"));

	std::string tempPort;
	
	if (tempHost.find_first_of(":") != std::string::npos)
	{
		tempPort = tempHost.substr(tempHost.find_first_of(":") + 1);
	}

	tempHost = tempHost.substr(0, tempHost.find_first_of(":"));

	auto addrPort = tempPort.empty() ? (std::string("https://").compare(str.substr(0, 8)) == 0 ? 443 : 80) : atoi(tempPort.c_str());
	return rtc::SocketAddress(tempHost, addrPort);
}

int OAuth24DProvider::ConnectSocket(rtc::SocketAddress addr)
{
	socket_.reset(new SslCapableSocket(addr.family(), addr.port() == 443, signaling_thread_));
	socket_->SignalCloseEvent.connect(this, &OAuth24DProvider::SocketClose);
	socket_->SignalConnectEvent.connect(this, &OAuth24DProvider::SocketOpen);
	socket_->SignalReadEvent.connect(this, &OAuth24DProvider::SocketRead);

	return socket_->Connect(addr);
}

bool OAuth24DProvider::Authenticate()
{
	if (state_ != State::NOT_ACTIVE)
	{
		return false;
	}

	if (code_host_.IsUnresolvedIP())
	{
		state_ = RESOLVING_CODE;
		resolver_.reset(new rtc::AsyncResolver());
		resolver_->SignalDone.connect(this, &OAuth24DProvider::AddressResolve);
		resolver_->Start(code_host_);

		return true;
	}

	if (poll_host_.IsUnresolvedIP())
	{
		state_ = RESOLVING_POLL;
		resolver_.reset(new rtc::AsyncResolver());
		resolver_->SignalDone.connect(this, &OAuth24DProvider::AddressResolve);
		resolver_->Start(poll_host_);

		return true;
	}

	state_ = State::REQUEST_CODE;

	int err = ConnectSocket(code_host_);

	return err != SOCKET_ERROR;
}

void OAuth24DProvider::OnMessage(rtc::Message* msg)
{
	// this indicates it's time to poll again
	if (msg->message_id == kPollMessageId)
	{
		ConnectSocket(poll_host_);
	}
}

void OAuth24DProvider::SocketOpen(rtc::AsyncSocket* socket)
{
	// issue the code query
	if (state_ == State::REQUEST_CODE)
	{
		auto data = "GET " + code_uri_ + " HTTP/1.1\r\n"
			"Host: " + code_host_.hostname() + "\r\n\r\n";

		auto sent = socket_->Send(data.c_str(), data.length());
		RTC_DCHECK(sent == data.length());
		
		state_ = State::ACTIVE_CODE;
	}
	// issue the poll query
	else if (state_ == State::REQUEST_POLL)
	{
		auto data = "GET " + poll_uri_ + "?device_code=" + data_.device_code + " HTTP/1.1\r\n"
			"Host: " + poll_host_.hostname() + "\r\n\r\n";

		auto sent = socket_->Send(data.c_str(), data.length());
		RTC_DCHECK(sent == data.length());

		state_ = State::ACTIVE_POLL;
	}
}

void OAuth24DProvider::SocketRead(rtc::AsyncSocket* socket)
{
	std::string data;

	if (state_ == State::ACTIVE_CODE || state_ == State::ACTIVE_POLL)
	{
		// parse response
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
	}

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
		if (state_ == State::ACTIVE_CODE)
		{
			auto deviceCodeWrapper = root.get("device_code", NULL);
			if (deviceCodeWrapper != NULL)
			{
				data_.device_code = deviceCodeWrapper.asString();
			}

			auto userCodeWrapper = root.get("user_code", NULL);
			if (userCodeWrapper != NULL)
			{
				data_.user_code = userCodeWrapper.asString();
			}

			auto intervalWrapper = root.get("interval", NULL);
			if (intervalWrapper != NULL)
			{
				data_.interval = intervalWrapper.asInt();
			}

			auto verificationUrlWrapper = root.get("verification_url", NULL);
			if (verificationUrlWrapper != NULL)
			{
				data_.verification_url = verificationUrlWrapper.asString();
			}

			// emit the code complete event
			SignalCodeComplete.emit(data_);

			// start polling
			state_ = State::REQUEST_POLL;

			socket_->Close();
			ConnectSocket(poll_host_);
		}
		else if (state_ == State::ACTIVE_POLL)
		{
			// get response status
			int status = -1;
			size_t pos = data.find(' ');
			if (pos != std::string::npos)
			{
				status = atoi(&data[pos + 1]);
			}

			if (status == 400)
			{
				// poll again
				state_ = State::REQUEST_POLL;

				socket_->Close();

				// schedule another poll at the interval from the CODE message
				signaling_thread_->PostDelayed(RTC_FROM_HERE, data_.interval * 1000, this, kPollMessageId);
			}
			else if (status == 200)
			{
				AuthenticationProviderResult completionData;
				completionData.successFlag = true;

				auto accessCodeWrapper = root.get("access_code", NULL);
				if (accessCodeWrapper != NULL)
				{
					completionData.accessToken = accessCodeWrapper.asString();
				}

				// emit the event
				SignalAuthenticationComplete.emit(completionData);

				// after emission, we can close our socket 
				// note: we don't really mind if this fails, it doesn't matter until 
				// we try another request, at which point we'll handle the error on that call 
				socket_->Close();

				// after emission, we're totally done, and move to a NOT_ACTIVE state to allow 
				// more authentication requests to be triggered 
				state_ = State::NOT_ACTIVE;
			}
		}
	}
}

void OAuth24DProvider::SocketClose(rtc::AsyncSocket* socket, int err)
{
	// TODO(bengreenier): remove this handler
	return;
}

void OAuth24DProvider::AddressResolve(rtc::AsyncResolverInterface* resolver)
{
	// capture the result
	auto tempHost = resolver->address();

	// release the resolver pointer and destroy the resolver
	auto released = resolver_.release();
	released->Destroy(true);

	// reset the resolver pointer to nullptr
	resolver_.reset(nullptr);

	if (state_ != State::RESOLVING_CODE && state_ != State::RESOLVING_POLL)
	{
		return;
	}

	if (state_ == State::RESOLVING_CODE)
	{
		code_host_ = tempHost;

		if (poll_host_.IsUnresolvedIP())
		{
			state_ = RESOLVING_POLL;
			resolver_.reset(new rtc::AsyncResolver());
			resolver_->SignalDone.connect(this, &OAuth24DProvider::AddressResolve);
			resolver_->Start(poll_host_);
			
			return;
		}
	}

	poll_host_ = tempHost;
	state_ = State::REQUEST_CODE;
	
	// connect the socket to code_host_ to REQUEST_CODE
	ConnectSocket(code_host_);
}