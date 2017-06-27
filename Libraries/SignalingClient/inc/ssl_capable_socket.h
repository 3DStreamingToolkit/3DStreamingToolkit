#ifndef WEBRTC_SSL_CAPABLE_SOCKET_H_
#define WEBRTC_SSL_CAPABLE_SOCKET_H_

#include "webrtc/base/socket.h"
#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/openssladapter.h"

using namespace rtc;

class SslCapableSocket : public AsyncSocket, public sigslot::has_slots<>
{
public:
	SslCapableSocket(const int& family, const bool& useSsl = false);
	~SslCapableSocket();

	void SetUseSsl(const bool& useSsl);
	bool GetUseSsl() const;

	SocketAddress GetLocalAddress() const;
	SocketAddress GetRemoteAddress() const;
	int Bind(const SocketAddress& addr);
	int Connect(const SocketAddress& addr);
	int Send(const void* pv, size_t cb);
	int SendTo(const void* pv, size_t cb, const SocketAddress& addr);
	int Recv(void* pv, size_t cb, int64_t* timestamp);
	int RecvFrom(void* pv,
		size_t cb,
		SocketAddress* paddr,
		int64_t* timestamp);
	int Listen(int backlog);
	AsyncSocket* Accept(SocketAddress* paddr);
	int Close();
	int GetError() const;
	void SetError(int error);
	Socket::ConnState GetState() const;
	int EstimateMTU(uint16_t* mtu);
	int GetOption(AsyncSocket::Option opt, int* value);
	int SetOption(AsyncSocket::Option opt, int value);
private:
	AsyncSocket* socket_;
	std::unique_ptr<SSLAdapter> ssl_adapter_;

	void MapUnderlyingEvents(AsyncSocket* provider, AsyncSocket* oldProvider = nullptr);

	void RefireReadEvent(AsyncSocket* socket);
	void RefireWriteEvent(AsyncSocket* socket);
	void RefireConnectEvent(AsyncSocket* socket);
	void RefireCloseEvent(AsyncSocket* socket, int err);
};

#endif  // WEBRTC_SSL_CAPABLE_SOCKET_H_