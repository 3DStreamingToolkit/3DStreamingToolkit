#ifndef WEBRTC_SSL_CAPABLE_SOCKET_H_
#define WEBRTC_SSL_CAPABLE_SOCKET_H_

#include "webrtc/rtc_base/ssladapter.h"
#include "webrtc/rtc_base/sslidentity.h"

#include "CppFactory.hpp"

using namespace rtc;

class SslCapableSocket : public AsyncSocket, public sigslot::has_slots<>
{
public:
	SslCapableSocket(const int& family, const bool& use_ssl, std::weak_ptr<Thread> signaling_thread);
	SslCapableSocket(std::unique_ptr<AsyncSocket> wrapped_socket, const bool& use_ssl, std::weak_ptr<Thread> signaling_thread);

	virtual ~SslCapableSocket();

	virtual void SetUseSsl(const bool& useSsl);
	virtual bool GetUseSsl() const;

	virtual SocketAddress GetLocalAddress() const;
	virtual SocketAddress GetRemoteAddress() const;
	virtual int Bind(const SocketAddress& addr);
	virtual int Connect(const SocketAddress& addr);
	virtual int Send(const void* pv, size_t cb);
	virtual int SendTo(const void* pv, size_t cb, const SocketAddress& addr);
	virtual int Recv(void* pv, size_t cb, int64_t* timestamp);
	virtual int RecvFrom(void* pv,
		size_t cb,
		SocketAddress* paddr,
		int64_t* timestamp);
	virtual int Listen(int backlog);
	virtual AsyncSocket* Accept(SocketAddress* paddr);
	virtual int Close();
	virtual int GetError() const;
	virtual void SetError(int error);
	virtual Socket::ConnState GetState() const;
	virtual int GetOption(AsyncSocket::Option opt, int* value);
	virtual int SetOption(AsyncSocket::Option opt, int value);

	typedef CppFactory::Factory<SslCapableSocket, const int&, const bool&, std::weak_ptr<Thread>> Factory;
protected:
	std::weak_ptr<rtc::Thread> signaling_thread_;
	std::unique_ptr<AsyncSocket> socket_;
	std::unique_ptr<SSLAdapter> ssl_adapter_;

	void MapUnderlyingEvents(AsyncSocket* provider, AsyncSocket* oldProvider = nullptr);

	void RefireReadEvent(AsyncSocket* socket);
	void RefireWriteEvent(AsyncSocket* socket);
	void RefireConnectEvent(AsyncSocket* socket);
	void RefireCloseEvent(AsyncSocket* socket, int err);
};

#endif  // WEBRTC_SSL_CAPABLE_SOCKET_H_