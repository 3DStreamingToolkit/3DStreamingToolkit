#include <gtest\gtest.h>
#include <gmock\gmock.h>

#include "peer_connection_client.h"
#include "turn_credential_provider.h"

#include "RtcEventLoop.h"
#include "Observers\NoFailureObserver.hpp"
#include "Observers\ConnectionObserver.hpp"

#pragma comment(lib, "webrtc.lib")
#pragma comment(lib, "Winmm.lib")

using namespace testing;
using namespace std;

class MockSslCapableSocketFactory : public SslCapableSocket::Factory
{
public:
	MOCK_METHOD3(Allocate, unique_ptr<SslCapableSocket>(const int&, const bool&, weak_ptr<Thread>));
};

class FakeSslCapableSocket : public SslCapableSocket, public rtc::MessageHandler
{
public:
	FakeSslCapableSocket(const string& data_str, const int& family, const bool& useSsl, std::weak_ptr<Thread> signalingThread) :
		SslCapableSocket(family, useSsl, signalingThread),
		data_str_(data_str),
		data_pos_(0),
		state_(Socket::ConnState::CS_CLOSED) {}

	int Connect(const SocketAddress& addr) override
	{
		state_ = Socket::ConnState::CS_CONNECTING;
		
		// simulate this taking some time
		if (auto marshalledThread = signaling_thread_.lock())
		{
			marshalledThread->PostDelayed(RTC_FROM_HERE, 1000, this);
		}

		return 0;
	}

	int Send(const void* pv, size_t cb) override
	{
		// simulate this taking some time
		if (auto marshalledThread = signaling_thread_.lock())
		{
			marshalledThread->PostDelayed(RTC_FROM_HERE, 1000, this);
		}

		return (int)cb;
	}

	int Recv(void* pv, size_t cb, int64_t* timestamp) override
	{
		// shortest amount of data we can populate
		auto smallest = cb < data_str_.length() ? cb : data_str_.length();

		if (data_pos_ < smallest)
		{
			data_str_.copy((char*)pv, smallest, data_pos_);
			data_pos_ = smallest;
			return (int)data_pos_;
		}
		else
		{
			return 0;
		}
	}

	int Close() override
	{
		state_ = Socket::ConnState::CS_CLOSED;
		
		// simulate this taking some time
		if (auto marshalledThread = signaling_thread_.lock())
		{
			marshalledThread->PostDelayed(RTC_FROM_HERE, 1000, this);
		}

		return 0;
	}

	Socket::ConnState GetState() const override
	{
		return state_;
	}

	void OnMessage(rtc::Message* msg) override
	{
		if (state_ == Socket::ConnState::CS_CONNECTING)
		{
			state_ = Socket::ConnState::CS_CONNECTED;
			SignalConnectEvent.emit(this);
		}
		else if (state_ == Socket::ConnState::CS_CLOSED)
		{
			RefireCloseEvent(this, 0);
		}
		else
		{
			RefireReadEvent(this);
		}
	}
protected:
	string data_str_;
	int data_pos_;

	Socket::ConnState state_;
};

class MockSslCapableSocket : public SslCapableSocket
{
public:
	MockSslCapableSocket(const int& family, const bool& useSsl, std::weak_ptr<Thread> signalingThread, const string& fake_data_str = "") :
		SslCapableSocket(family, useSsl, signalingThread),
		fake_(fake_data_str, family, useSsl, signalingThread) {}

	MOCK_METHOD1(SetUseSsl, void(const bool& useSsl));
	MOCK_CONST_METHOD0(GetUseSsl, bool());

	MOCK_CONST_METHOD0(GetLocalAddress, SocketAddress());
	MOCK_CONST_METHOD0(GetRemoteAddress, SocketAddress());
	MOCK_METHOD1(Bind, int(const SocketAddress& addr));
	MOCK_METHOD1(Connect, int(const SocketAddress& addr));
	MOCK_METHOD2(Send, int(const void* pv, size_t cb));
	MOCK_METHOD3(SendTo, int(const void* pv, size_t cb, const SocketAddress& addr));
	MOCK_METHOD3(Recv, int(void* pv, size_t cb, int64_t* timestamp));
	MOCK_METHOD4(RecvFrom, int(void* pv,
		size_t cb,
		SocketAddress* paddr,
		int64_t* timestamp));
	MOCK_METHOD1(Listen, int(int backlog));
	MOCK_METHOD1(Accept, AsyncSocket*(SocketAddress* paddr));
	MOCK_METHOD0(Close, int());
	MOCK_CONST_METHOD0(GetError, int());
	MOCK_METHOD1(SetError, void(int error));
	MOCK_CONST_METHOD0(GetState, Socket::ConnState());
	MOCK_METHOD2(GetOption, int(AsyncSocket::Option opt, int* value));
	MOCK_METHOD2(SetOption, int(AsyncSocket::Option opt, int value));

	// Delegates the default actions of the methods to a FakeFoo object.
	// This must be called *before* the custom ON_CALL() statements.
	void DelegateToFake()
	{
		ON_CALL(*this, Connect(_))
			.WillByDefault(Invoke(&fake_, &FakeSslCapableSocket::Connect));
		ON_CALL(*this, Send(_, _))
			.WillByDefault(Invoke(&fake_, &FakeSslCapableSocket::Send));
		ON_CALL(*this, Recv(_, _, _))
			.WillByDefault(Invoke(&fake_, &FakeSslCapableSocket::Recv));
		ON_CALL(*this, Close())
			.WillByDefault(Invoke(&fake_, &FakeSslCapableSocket::Close));
		ON_CALL(*this, GetState())
			.WillByDefault(Invoke(&fake_, &FakeSslCapableSocket::GetState));

		// map in the fake event emitters
		MapUnderlyingEvents(&fake_);
	}
private:
	FakeSslCapableSocket fake_;
};

class MockAsyncSocket : public rtc::AsyncSocket
{
public:
	MOCK_CONST_METHOD0(GetLocalAddress, SocketAddress());
	MOCK_CONST_METHOD0(GetRemoteAddress, SocketAddress());
	MOCK_METHOD1(Bind, int(const SocketAddress& addr));
	MOCK_METHOD1(Connect, int(const SocketAddress& addr));
	MOCK_METHOD2(Send, int(const void* pv, size_t cb));
	MOCK_METHOD3(SendTo, int(const void* pv, size_t cb, const SocketAddress& addr));
	MOCK_METHOD3(Recv, int(void* pv, size_t cb, int64_t* timestamp));
	MOCK_METHOD4(RecvFrom, int(void* pv,
		size_t cb,
		SocketAddress* paddr,
		int64_t* timestamp));
	MOCK_METHOD1(Listen, int(int backlog));
	MOCK_METHOD1(Accept, AsyncSocket*(SocketAddress* paddr));
	MOCK_METHOD0(Close, int());
	MOCK_CONST_METHOD0(GetError, int());
	MOCK_METHOD1(SetError, void(int error));
	MOCK_CONST_METHOD0(GetState, Socket::ConnState());
	MOCK_METHOD2(GetOption, int(AsyncSocket::Option opt, int* value));
	MOCK_METHOD2(SetOption, int(AsyncSocket::Option opt, int value));
};

class MockAuthenticationProvider : public AuthenticationProvider
{
public:
	MOCK_METHOD0(Authenticate, bool());
};

/// <summary>
/// Validate that peer_connection_client can correctly create sockets
/// </summary>
TEST(SignalingClientTests, AllocateInvoked)
{
	shared_ptr<MockSslCapableSocketFactory> factory = make_shared<MockSslCapableSocketFactory>();
	NoFailureObserver obs;

	// expect 4 socket allocations that do a normal allocation under the hood
	EXPECT_CALL(*factory, Allocate(_, _, _))
		.Times(Exactly(4))
		.WillRepeatedly(Invoke([](const int& a, const bool& b, weak_ptr<Thread> c) { return make_unique<SslCapableSocket>(a, b, c); }));

	// scope for loop guard
	{
		// tie client lifetime to loop guard
		shared_ptr<PeerConnectionClient> client;
		RtcEventLoop loop([&]()
		{
			// alloc client, bind observer
			client = make_shared<PeerConnectionClient>(factory);
			client->RegisterObserver(&obs);

			// attempt valid connect
			client->Connect("localhost", 1, "test");
		});

		// block test thread waiting for observer
		// expect the observer to be truthy, indicating there are no ServerConnectionFailures
		EXPECT_TRUE(obs.Wait());

		// rely on RAII to kill the loop
	}
}

/// <summary>
/// Validate that SslCapableSocket is a light shim on top of AsyncSocket and passes calls through
/// </summary>
TEST(SignalingClient, SslCapableSocketPassthrough)
{
	auto mockSocket = std::make_unique<MockAsyncSocket>();
	auto mockThread = std::make_shared<rtc::Thread>();

	// expect that each method will be called once
	EXPECT_CALL(*mockSocket, GetLocalAddress()).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, GetRemoteAddress()).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Bind(_)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Connect(_)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Send(_, _)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, SendTo(_, _, _)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Recv(_, _, _)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, RecvFrom(_, _, _, _)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Listen(_)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Accept(_)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, Close()).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, GetError()).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, SetError(_)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, GetState()).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, GetOption(_, _)).Times(Exactly(1));
	EXPECT_CALL(*mockSocket, SetOption(_, _)).Times(Exactly(1));

	// note: this was nontrivial to me, but move does NOT break gmock assertions
	SslCapableSocket socket(std::move(mockSocket), false, std::weak_ptr<rtc::Thread>(mockThread));

	// call all socket methods to ensure passthrough is working
	socket.GetLocalAddress();
	socket.GetRemoteAddress();

	rtc::SocketAddress addr;
	socket.Bind(addr);
	socket.Connect(addr);
	socket.SendTo(nullptr, (size_t)0, addr);

	socket.Send(nullptr, (size_t)0);
	socket.Recv(nullptr, (size_t)0, nullptr);
	socket.RecvFrom(nullptr, (size_t)0, nullptr, nullptr);
	socket.Listen(0);
	socket.Accept(nullptr);
	socket.Close();
	socket.GetError();
	socket.SetError(0);
	socket.GetState();

	// opt is an enum
	auto opt = (rtc::Socket::Option)0;
	socket.GetOption(opt, nullptr);
	socket.SetOption(opt, 0);
}

/// <summary>
/// Validate that peer_connection_client can correctly send heartbeats
/// </summary>
TEST(SignalingClientTests, HeartbeatConnect)
{
	shared_ptr<MockSslCapableSocketFactory> factory = make_shared<MockSslCapableSocketFactory>();
	ConnectionObserver obs;

	// make the mock allocator allocate mock sockets
	ON_CALL(*factory, Allocate(_, _, _))
		.WillByDefault(Invoke([](const int& a, const bool& b, weak_ptr<Thread> c)
	{
		const auto fake_data_str = "HTTP/1.1 200 OK\r\n";
		auto mockSocket = make_unique<MockSslCapableSocket>(a, b, c, fake_data_str);

		mockSocket->DelegateToFake();

		// use this mocked instance
		return mockSocket;
	}));

	// make the mock allocator allocate the first socket to do real connect socket things
	EXPECT_CALL(*factory, Allocate(_, _, _))
		.Times(AtLeast(1))
		.WillOnce(Invoke([](const int& a, const bool& b, weak_ptr<Thread> c)
	{
		// configure the data our fake will send back as a response
		const auto fake_data_str = "HTTP/1.1 200 OK\r\nX-Powered-By: Express\r\nPragma: 2\r\nContent-Type: text/plain;charset=utf-8\r\nContent-Length: 18\r\nETag: W/\"12-mDfvg2OymdSB0T1Fwl+XHiLarp8\"\r\nConnection: keep-alive\r\n\r\ntest, 2, 1\r\nother, 1, 0\r\n\0";
		auto mockSocket = make_unique<MockSslCapableSocket>(a, b, c, fake_data_str);

		mockSocket->DelegateToFake();

		// use this mocked instance
		return mockSocket;
	}));

	// scope for loop guard
	{
		// tie client lifetime to loop guard
		shared_ptr<PeerConnectionClient> client;
		RtcEventLoop loop([&]()
		{
			// alloc client, bind observer
			client = make_shared<PeerConnectionClient>(factory);
			client->RegisterObserver(&obs);

			// attempt valid connect
			client->Connect("localhost", 1, "test");
		});

		// block test thread waiting for observer
		// expect the observer to be truthy, indicating there are no ServerConnectionFailures
		EXPECT_TRUE(obs.Wait());

		// ensure that the client correctly read the sign_in data
		// see FakeSslCapableSocket::data_str_ for where this data comes from
		ASSERT_EQ(client->id(), 2);
		ASSERT_STREQ(client->peers().at(1).c_str(), "other");

		// rely on RAII to kill the loop
	}
}

/// <summary>
/// Validate that peer_connection_client can correctly sign_in to a signaling server
/// </summary>
TEST(SignalingClientTests, SignalConnect)
{
	shared_ptr<MockSslCapableSocketFactory> factory = make_shared<MockSslCapableSocketFactory>();
	ConnectionObserver obs;

	// make the mock allocator allocate mock sockets
	ON_CALL(*factory, Allocate(_, _, _))
		.WillByDefault(Invoke([](const int& a, const bool& b , weak_ptr<Thread> c)
	{
		// configure the data our fake will send back as a response
		const auto fake_data_str = "HTTP/1.1 200 OK\r\nX-Powered-By: Express\r\nPragma: 2\r\nContent-Type: text/plain;charset=utf-8\r\nContent-Length: 18\r\nETag: W/\"12-mDfvg2OymdSB0T1Fwl+XHiLarp8\"\r\nConnection: keep-alive\r\n\r\ntest, 2, 1\r\nother, 1, 0\r\n\0";
		auto mockSocket = make_unique<MockSslCapableSocket>(a, b, c, fake_data_str);

		// make the mock socket use the fake for some critical behaviors
		// this is fine to apply to all sockets since we don't care about
		// anything except the control_socket (that hits /sign_in)
		mockSocket->DelegateToFake();

		// use this mocked instance
		return mockSocket;
	}));

	// scope for loop guard
	{
		// tie client lifetime to loop guard
		shared_ptr<PeerConnectionClient> client;
		RtcEventLoop loop([&]()
		{
			// alloc client, bind observer
			client = make_shared<PeerConnectionClient>(factory);
			client->RegisterObserver(&obs);

			// attempt valid connect
			client->Connect("localhost", 1, "test");
		});

		// block test thread waiting for observer
		// expect the observer to be truthy, indicating there are no ServerConnectionFailures
		EXPECT_TRUE(obs.Wait());

		// ensure that the client correctly read the sign_in data
		// see fake_data_str
		ASSERT_EQ(client->id(), 2);
		ASSERT_STREQ(client->peers().at(1).c_str(), "other");

		// rely on RAII to kill the loop
	}
}

/// <summary>
/// Validate that turn_credential_provider can provide credentials
/// </summary>
TEST(SignalingClient, TurnCredentialProviderSuccess)
{
	shared_ptr<MockSslCapableSocketFactory> factory = make_shared<MockSslCapableSocketFactory>();
	MockAuthenticationProvider mockProvider;
	NoFailureObserver obs;

	// expect 1 socket allocation that does mock allocations under the hood
	EXPECT_CALL(*factory, Allocate(_, _, _))
		.Times(Exactly(1))
		.WillRepeatedly(Invoke([](const int& a, const bool& b, weak_ptr<Thread> c)
	{
		const auto fake_data_str = "HTTP/1.1 200 OK\r\n\r\n{\"username\":\"test\", \"password\":\"secure123\"}";
		auto mockSocket = make_unique<MockSslCapableSocket>(a, b, c, fake_data_str);

		// make the mock socket use the fake for some critical behaviors
		mockSocket->DelegateToFake();

		// use this mocked instance
		return mockSocket;
	}));

	// create a signal to block the main thread while webrtc threads operate
	rtc::Event block_for_turn(true, false);
	bool turnResult;

	TurnCredentialProvider::CredentialsRetrievedCallback cb([&](const TurnCredentials& result)
	{
		// TODO(bengreenier): assert mock values
		ASSERT_STREQ(result.username.c_str(), "test");
		ASSERT_STREQ(result.password.c_str(), "secure123");

		turnResult = result.successFlag;
		block_for_turn.Set();
	});

	// scope for loop guard
	{
		// tie client lifetime to loop guard
		shared_ptr<TurnCredentialProvider> client;

		// expect that we shim to the underlying provider exactly once
		EXPECT_CALL(mockProvider, Authenticate())
			.Times(Exactly(1))
			.WillRepeatedly(Invoke([&]()
		{
			// make it report back that auth is a-ok
			AuthenticationProviderResult res;
			res.successFlag = true;
			res.accessToken = "testToken";
			client->OnAuthenticationComplete(res);

			return true;
		}));

		RtcEventLoop loop([&]()
		{
			// alloc client, bind observer
			client = make_shared<TurnCredentialProvider>("http://localhost", factory);

			client->SetAuthenticationProvider(&mockProvider);

			ASSERT_EQ(client->state(), TurnCredentialProvider::State::NOT_ACTIVE);

			client->SignalCredentialsRetrieved.connect(&cb, &TurnCredentialProvider::CredentialsRetrievedCallback::Handle);
			
			ASSERT_TRUE(client->RequestCredentials());

			// this is or-ed because we could have either case and we're best-effort testing for now
			// TODO(bengreenier): factor into two different tests
			EXPECT_TRUE(client->state() == TurnCredentialProvider::State::AUTHENTICATING ||
				client->state() == TurnCredentialProvider::State::RESOLVING);
		});

		// block test thread waiting for observer
		EXPECT_TRUE(block_for_turn.Wait(rtc::Event::kForever) && turnResult);

		// rely on RAII to kill the loop
	}
}
