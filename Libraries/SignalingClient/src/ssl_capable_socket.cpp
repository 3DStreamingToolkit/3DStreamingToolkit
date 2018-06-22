#include "ssl_capable_socket.h"

#include <chrono>

#ifdef WIN32
#include "webrtc/rtc_base/win32socketserver.h"
#endif

namespace
{
	rtc::AsyncSocket* CreateClientSocket(int family)
	{
#ifdef WIN32
		rtc::Win32Socket* sock = new rtc::Win32Socket();
		sock->CreateT(family, SOCK_STREAM);
		return sock;
#elif defined(WEBRTC_POSIX) // WIN32
		rtc::Thread* thread = rtc::Thread::Current();
		RTC_DCHECK(thread != NULL);
		return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
#else // WIN32
#error Platform not supported.
#endif // WIN32
	}
}

SslCapableSocket::SslCapableSocket(const int& family, const bool& use_ssl, std::weak_ptr<Thread> signaling_thread) :
	SslCapableSocket(std::unique_ptr<AsyncSocket>(CreateClientSocket(family)), use_ssl, signaling_thread)
{
}

SslCapableSocket::SslCapableSocket(std::unique_ptr<AsyncSocket> wrapped_socket, const bool& use_ssl, std::weak_ptr<Thread> signaling_thread) :
	socket_(std::move(wrapped_socket)),
	ssl_adapter_(nullptr),
	signaling_thread_(signaling_thread)
{
	MapUnderlyingEvents(socket_.get());
	SetUseSsl(use_ssl);
}

SslCapableSocket::~SslCapableSocket()
{
	if (ssl_adapter_.get() == nullptr)
	{
		delete socket_.release();
	}
}

void SslCapableSocket::SetUseSsl(const bool& useSsl)
{
	if (useSsl && ssl_adapter_.get() == nullptr)
	{
		ssl_adapter_.reset(SSLAdapter::Create(socket_.get()));
		ssl_adapter_->SetMode(rtc::SSL_MODE_TLS);
		MapUnderlyingEvents(ssl_adapter_.get(), socket_.get());
	}
	else if (!useSsl && ssl_adapter_.get() != nullptr)
	{
		ssl_adapter_.reset(nullptr);
		MapUnderlyingEvents(socket_.get());
	}
}

bool SslCapableSocket::GetUseSsl() const
{
	return ssl_adapter_.get() == nullptr;
}

SocketAddress SslCapableSocket::GetLocalAddress() const
{
	return ssl_adapter_.get() == nullptr ? socket_->GetLocalAddress() : ssl_adapter_->GetLocalAddress();
}

SocketAddress SslCapableSocket::GetRemoteAddress() const
{
	return ssl_adapter_.get() == nullptr ? socket_->GetRemoteAddress() : ssl_adapter_->GetRemoteAddress();
}

int SslCapableSocket::Bind(const SocketAddress& addr)
{
	return ssl_adapter_.get() == nullptr ? socket_->Bind(addr) : ssl_adapter_->Bind(addr);
}

int SslCapableSocket::Connect(const SocketAddress& addr)
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	if (ssl_adapter_.get() == nullptr)
	{
		return socket_->Connect(addr);
	}
	else
	{
		int err = ssl_adapter_->StartSSL(addr.hostname().c_str(), false);

		if (err == 0)
		{
			err = ssl_adapter_->Connect(addr);
		}

		return err;
	}
}

int SslCapableSocket::Send(const void* pv, size_t cb)
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	return ssl_adapter_.get() == nullptr ? socket_->Send(pv, cb) : ssl_adapter_->Send(pv, cb);
}

int SslCapableSocket::SendTo(const void* pv, size_t cb, const SocketAddress& addr)
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	return ssl_adapter_.get() == nullptr ? socket_->SendTo(pv, cb, addr) : ssl_adapter_->SendTo(pv, cb, addr);
}

int SslCapableSocket::Recv(void* pv, size_t cb, int64_t* timestamp)
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	return ssl_adapter_.get() == nullptr ? socket_->Recv(pv, cb, timestamp) : ssl_adapter_->Recv(pv, cb, timestamp);
}

int SslCapableSocket::RecvFrom(void* pv,
	size_t cb,
	SocketAddress* paddr,
	int64_t* timestamp)
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	return ssl_adapter_.get() == nullptr ? socket_->RecvFrom(pv, cb, paddr, timestamp) : ssl_adapter_->RecvFrom(pv, cb, paddr, timestamp);
}

int SslCapableSocket::Listen(int backlog)
{
	return ssl_adapter_.get() == nullptr ? socket_->Listen(backlog) : ssl_adapter_->Listen(backlog);
}

AsyncSocket* SslCapableSocket::Accept(SocketAddress* paddr)
{
	return ssl_adapter_.get() == nullptr ? socket_->Accept(paddr) : ssl_adapter_->Accept(paddr);
}

int SslCapableSocket::Close()
{
	LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	return ssl_adapter_.get() == nullptr ? socket_->Close() : ssl_adapter_->Close();
}

int SslCapableSocket::GetError() const
{
	return ssl_adapter_.get() == nullptr ? socket_->GetError() : ssl_adapter_->GetError();
}

void SslCapableSocket::SetError(int error)
{
	return ssl_adapter_.get() == nullptr ? socket_->SetError(error) : ssl_adapter_->SetError(error);
}

Socket::ConnState SslCapableSocket::GetState() const
{
	return ssl_adapter_.get() == nullptr ? socket_->GetState() : ssl_adapter_->GetState();
}

int SslCapableSocket::GetOption(AsyncSocket::Option opt, int* value)
{
	return ssl_adapter_.get() == nullptr ? socket_->GetOption(opt, value) : ssl_adapter_->GetOption(opt, value);
}

int SslCapableSocket::SetOption(AsyncSocket::Option opt, int value)
{
	return ssl_adapter_.get() == nullptr ? socket_->SetOption(opt, value) : ssl_adapter_->SetOption(opt, value);
}

void SslCapableSocket::MapUnderlyingEvents(AsyncSocket* provider, AsyncSocket* oldProvider)
{
	if (oldProvider != nullptr)
	{
		oldProvider->SignalReadEvent.disconnect(this);
		oldProvider->SignalWriteEvent.disconnect(this);
		oldProvider->SignalConnectEvent.disconnect(this);
		oldProvider->SignalCloseEvent.disconnect(this);
	}

	provider->SignalReadEvent.connect(this, &SslCapableSocket::RefireReadEvent);
	provider->SignalWriteEvent.connect(this, &SslCapableSocket::RefireWriteEvent);
	provider->SignalConnectEvent.connect(this, &SslCapableSocket::RefireConnectEvent);
	provider->SignalCloseEvent.connect(this, &SslCapableSocket::RefireCloseEvent);
}

void SslCapableSocket::RefireReadEvent(AsyncSocket* socket)
{
	if (auto marshaled_thread = signaling_thread_.lock())
	{
		marshaled_thread->Invoke<void>(RTC_FROM_HERE, [&]
		{
			LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			this->SignalReadEvent.emit(socket);
		});
	}
}

void SslCapableSocket::RefireWriteEvent(AsyncSocket* socket)
{
	if (auto marshaled_thread = signaling_thread_.lock())
	{
		marshaled_thread->Invoke<void>(RTC_FROM_HERE, [&]
		{
			LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			this->SignalWriteEvent.emit(socket);
		});
	}
}

void SslCapableSocket::RefireConnectEvent(AsyncSocket* socket)
{
	if (auto marshaled_thread = signaling_thread_.lock())
	{
		marshaled_thread->Invoke<void>(RTC_FROM_HERE, [&]
		{
			LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			this->SignalConnectEvent.emit(socket);
		});
	}
}

void SslCapableSocket::RefireCloseEvent(AsyncSocket* socket, int err)
{
	if (auto marshaled_thread = signaling_thread_.lock())
	{
		marshaled_thread->Invoke<void>(RTC_FROM_HERE, [&]
		{
			LOG(INFO) << __FUNCTION__ << "@" << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

			this->SignalCloseEvent.emit(socket, err);
		});
	}
}