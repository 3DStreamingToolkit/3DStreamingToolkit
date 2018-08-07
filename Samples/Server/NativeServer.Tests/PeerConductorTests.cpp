#include <gtest\gtest.h>
#include <gmock\gmock.h>

#include "peer_conductor.h"
#include "multi_peer_conductor.h"

using namespace ::testing;

class PeerConductorFixture : public PeerConductor
{
public:
	PeerConductorFixture(scoped_refptr<PeerConnectionFactoryInterface> peer_factory) :
		PeerConductorFixture(-1, "", make_shared<WebRTCConfig>(), peer_factory, [](const std::string&) {})
	{
	}

	PeerConductorFixture(scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
		const function<void(const string&)>& send_func) :
		PeerConductorFixture(-1, "", make_shared<WebRTCConfig>(), peer_factory, send_func)
	{
	}

	PeerConductorFixture(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtc_config,
		scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
		const function<void(const string&)>& send_func) :
		PeerConductor(id, name, webrtc_config, peer_factory, send_func)
	{
	}

	void Test_SetPeerConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc)
	{
		peer_connection_ = pc;
	}

	MOCK_METHOD0(AllocateVideoCapturer, unique_ptr<cricket::VideoCapturer>());
};

class VideoCapturerFixture : public cricket::VideoCapturer
{
public:
	MOCK_METHOD1(Start, cricket::CaptureState(const cricket::VideoFormat&));
	MOCK_METHOD0(Stop, void());
	MOCK_METHOD0(IsRunning, bool());
	MOCK_CONST_METHOD0(IsScreencast, bool());
	MOCK_METHOD1(GetPreferredFourccs, bool(std::vector<uint32_t>*));
};

class MultiPeerConductorFixture : public MultiPeerConductor
{
public:
	// internal peer conductor fixture to always return true for IsConnected
	class IntPeerConductorFixture : public PeerConductorFixture
	{
	public:
		IntPeerConductorFixture(scoped_refptr<PeerConnectionFactoryInterface> peer_factory) :
			PeerConductorFixture(peer_factory)
		{
		}

		virtual const bool IsConnected() const override
		{
			return true;
		}
	};

	MultiPeerConductorFixture(scoped_refptr<PeerConnectionFactoryInterface> peer_factory) :
		MultiPeerConductor(make_shared<FullServerConfigFixture>(), peer_factory)
	{
	}

	virtual void OnMessage(rtc::Message*) override {}

	virtual scoped_refptr<PeerConductor> SafeAllocatePeerMapEntry(int peer_id) override
	{
		// use an int fixture so that IsConnected can be truthy
		auto intFixture = new RefCountedObject<IntPeerConductorFixture>(peer_factory_);
		
		// mirror the workload of storing in peers
		connected_peers_[peer_id] = intFixture;

		// fire the counter hook so we can validate calls
		SafeAllocatePeerMapEntry_Counter();

		return connected_peers_[peer_id];
	}

	MOCK_METHOD0(SafeAllocatePeerMapEntry_Counter, void());
private:
	struct FullServerConfigFixture : public FullServerConfig
	{
		FullServerConfigFixture() {
			server_config = make_shared<ServerConfig>();
			webrtc_config = make_shared<WebRTCConfig>();
		}
	};
};

class PeerConnectionFactoryInterfaceFixture : public PeerConnectionFactoryInterface
{
public:
	MOCK_METHOD1(SetOptions, void(const PeerConnectionFactoryInterface::Options&));

	// see https://stackoverflow.com/questions/7616475/can-google-mock-a-method-with-a-smart-pointer-return-type
	// to understand this mocking pattern and why it's a requirement here

	virtual scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
		const PeerConnectionInterface::RTCConfiguration& configuration,
		std::unique_ptr<cricket::PortAllocator> allocator,
		std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
		PeerConnectionObserver* observer)
	{
		return CreatePeerConnection_RawPtr(configuration, allocator.get(), cert_generator.get(), observer);
	}

	MOCK_METHOD4(CreatePeerConnection_RawPtr, rtc::scoped_refptr<PeerConnectionInterface>(
		const PeerConnectionInterface::RTCConfiguration&,
		cricket::PortAllocator*,
		rtc::RTCCertificateGeneratorInterface*,
		PeerConnectionObserver*));

	virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
		const PeerConnectionInterface::RTCConfiguration& configuration,
		const MediaConstraintsInterface* constraints,
		std::unique_ptr<cricket::PortAllocator> allocator,
		std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator,
		PeerConnectionObserver* observer)
	{
		return CreatePeerConnection_RawPtr(configuration, constraints, allocator.get(), cert_generator.get(), observer);
	}

	MOCK_METHOD5(CreatePeerConnection_RawPtr, rtc::scoped_refptr<PeerConnectionInterface>(
		const PeerConnectionInterface::RTCConfiguration&,
		const MediaConstraintsInterface*,
		cricket::PortAllocator*,
		rtc::RTCCertificateGeneratorInterface*,
		PeerConnectionObserver*));

	virtual rtc::scoped_refptr<VideoTrackSourceInterface> CreateVideoSource(
		std::unique_ptr<cricket::VideoCapturer> capturer,
		const MediaConstraintsInterface* constraints)
	{
		return CreateVideoSource_RawPtr(capturer.get(), constraints);
	}

	MOCK_METHOD2(CreateVideoSource_RawPtr, rtc::scoped_refptr<VideoTrackSourceInterface>(
		cricket::VideoCapturer*,
		const MediaConstraintsInterface*));

	MOCK_METHOD1(CreateLocalMediaStream, rtc::scoped_refptr<MediaStreamInterface>(const std::string&));
	MOCK_METHOD1(CreateAudioSource, rtc::scoped_refptr<AudioSourceInterface>(const cricket::AudioOptions&));
	MOCK_METHOD1(CreateAudioSource, rtc::scoped_refptr<AudioSourceInterface>(const MediaConstraintsInterface*));
	MOCK_METHOD2(CreateVideoTrack, rtc::scoped_refptr<VideoTrackInterface>(const std::string&, VideoTrackSourceInterface*));
	MOCK_METHOD2(CreateAudioTrack, rtc::scoped_refptr<AudioTrackInterface>(const std::string&, AudioSourceInterface*));

	MOCK_METHOD2(StartAecDump, bool(rtc::PlatformFile, int64_t));
	MOCK_METHOD0(StopAecDump, void());

	MOCK_METHOD2(StartRtcEventLog, bool(rtc::PlatformFile, int64_t));
	MOCK_METHOD1(StartRtcEventLog, bool(rtc::PlatformFile));
	MOCK_METHOD0(StopRtcEventLog, void());
};

class PeerConnectionInterfaceFixture : public PeerConnectionInterface
{
public:
	MOCK_METHOD0(local_streams, rtc::scoped_refptr<webrtc::StreamCollectionInterface>());
	MOCK_METHOD0(remote_streams, rtc::scoped_refptr<webrtc::StreamCollectionInterface>());
	MOCK_METHOD1(AddStream, bool(webrtc::MediaStreamInterface*));
	MOCK_METHOD1(RemoveStream, void(webrtc::MediaStreamInterface*));
	MOCK_METHOD2(AddTrack, rtc::scoped_refptr<webrtc::RtpSenderInterface>(
		webrtc::MediaStreamTrackInterface*,
		std::vector<webrtc::MediaStreamInterface*>));
	MOCK_METHOD1(RemoveTrack, bool(webrtc::RtpSenderInterface*));
	MOCK_METHOD1(CreateDtmfSender, rtc::scoped_refptr<webrtc::DtmfSenderInterface>(
		webrtc::AudioTrackInterface*));
	MOCK_METHOD3(GetStats, bool(webrtc::StatsObserver*,
		webrtc::MediaStreamTrackInterface*,
		webrtc::PeerConnectionInterface::StatsOutputLevel));
	MOCK_METHOD2(CreateDataChannel, rtc::scoped_refptr<webrtc::DataChannelInterface>(
		const std::string&,
		const webrtc::DataChannelInit*));
	MOCK_CONST_METHOD0(local_description, webrtc::SessionDescriptionInterface*());
	MOCK_CONST_METHOD0(remote_description, webrtc::SessionDescriptionInterface*());
	MOCK_METHOD2(SetLocalDescription, void(webrtc::SetSessionDescriptionObserver*,
		webrtc::SessionDescriptionInterface* desc));
	MOCK_METHOD2(SetRemoteDescription, void(webrtc::SetSessionDescriptionObserver*,
		webrtc::SessionDescriptionInterface*));
	MOCK_METHOD1(AddIceCandidate, bool(const webrtc::IceCandidateInterface*));
	MOCK_METHOD1(RegisterUMAObserver, void(webrtc::UMAObserver*));
	MOCK_METHOD1(SetBitrate, webrtc::RTCError(
		const webrtc::PeerConnectionInterface::BitrateParameters&));
	MOCK_METHOD0(signaling_state, webrtc::PeerConnectionInterface::SignalingState());
	MOCK_METHOD0(ice_connection_state, webrtc::PeerConnectionInterface::IceConnectionState());
	MOCK_METHOD0(ice_gathering_state, webrtc::PeerConnectionInterface::IceGatheringState());
	MOCK_METHOD0(Close, void());
	MOCK_METHOD2(CreateAnswer, void(webrtc::CreateSessionDescriptionObserver*, const webrtc::MediaConstraintsInterface*));
};

class SessionDescriptionInterfaceFixture : public SessionDescriptionInterface
{
public:
	MOCK_METHOD0(description, cricket::SessionDescription*());
	MOCK_CONST_METHOD0(description, cricket::SessionDescription*());
	MOCK_CONST_METHOD0(session_id, std::string());
	MOCK_CONST_METHOD0(session_version, std::string());
	MOCK_CONST_METHOD0(type, std::string());
	MOCK_METHOD1(AddCandidate, bool(const IceCandidateInterface*));
	MOCK_METHOD1(RemoveCandidates, size_t(const std::vector<cricket::Candidate>&));
	MOCK_CONST_METHOD0(number_of_mediasections, size_t());
	MOCK_CONST_METHOD1(candidates, IceCandidateCollection*(size_t));
	MOCK_CONST_METHOD1(ToString, bool(std::string*));
};

class IceCandidateInterfaceFixture : public IceCandidateInterface
{
public:
	MOCK_CONST_METHOD0(sdp_mid, std::string());
	MOCK_CONST_METHOD0(sdp_mline_index, int());
	MOCK_CONST_METHOD0(candidate, cricket::Candidate&());
	MOCK_CONST_METHOD0(server_url, std::string());
	MOCK_CONST_METHOD1(ToString, bool(std::string*));
};

class MediaStreamInterfaceFixture : public MediaStreamInterface
{
public:
	MOCK_CONST_METHOD0(label, std::string());

	MOCK_METHOD0(GetAudioTracks, AudioTrackVector());
	MOCK_METHOD0(GetVideoTracks, VideoTrackVector());
	MOCK_METHOD1(FindAudioTrack, rtc::scoped_refptr<AudioTrackInterface>
		(const std::string&));
	MOCK_METHOD1(FindVideoTrack, rtc::scoped_refptr<VideoTrackInterface>
		(const std::string&));

	MOCK_METHOD1(AddTrack, bool(webrtc::AudioTrackInterface*));
	MOCK_METHOD1(AddTrack, bool(webrtc::VideoTrackInterface*));
	MOCK_METHOD1(RemoveTrack, bool(webrtc::AudioTrackInterface*));
	MOCK_METHOD1(RemoveTrack, bool(webrtc::VideoTrackInterface*));

	MOCK_METHOD1(RegisterObserver, void(ObserverInterface*));
	MOCK_METHOD1(UnregisterObserver, void(ObserverInterface*));
};

TEST(PeerConductorTests, PeerConductor_Alloc_Success)
{
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto streamFixture = new rtc::RefCountedObject<MediaStreamInterfaceFixture>();
	auto connFixture = new rtc::RefCountedObject<PeerConnectionInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture);

	// the following is the underlying allocation flow, mocked, and in order
	{
		InSequence seq;

		EXPECT_CALL(*factoryFixture, CreatePeerConnection_RawPtr(_, _, _, _, _))
			.Times(Exactly(1))
			.WillOnce(Invoke([&](const PeerConnectionInterface::RTCConfiguration& configuration,
				const webrtc::MediaConstraintsInterface* constraints,
				cricket::PortAllocator* allocator,
				rtc::RTCCertificateGeneratorInterface* cert_generator,
				PeerConnectionObserver* observer) {
			return connFixture;
		}));
		EXPECT_CALL(*fixture, AllocateVideoCapturer())
			.Times(Exactly(1));
		EXPECT_CALL(*factoryFixture, CreateVideoSource_RawPtr(_, _))
			.Times(Exactly(1));
		EXPECT_CALL(*factoryFixture, CreateVideoTrack(_, _))
			.Times(Exactly(1));
		EXPECT_CALL(*factoryFixture, CreateLocalMediaStream(_))
			.WillOnce(Invoke([&](std::string) {
			return streamFixture;
		}));
		EXPECT_CALL(*streamFixture, AddTrack(A<webrtc::VideoTrackInterface*>()))
			.Times(Exactly(1));
		EXPECT_CALL(*connFixture, AddStream(A<webrtc::MediaStreamInterface*>()))
			.Times(Exactly(1));
	}

	// alloc a conn
	fixture->AllocatePeerConnection();

	// assert that the conn is now there
	ASSERT_EQ(fixture->Streams().size(), 1);
}

TEST(PeerConductorTests, PeerConductor_SDPGeneration_Success)
{
	auto expectedContents = std::string("test message contents");
	auto expectedSdp = "{\n   \"password\" : \"\",\n   \"sdp\" : \"" + expectedContents + "\",\n   \"type\" : \"\",\n   \"uri\" : \"\",\n   \"username\" : \"\"\n}\n";
	auto wasSendFuncCalled = false;
	auto mockSendFunc = [&](const std::string& message)
	{
		wasSendFuncCalled = true;
		ASSERT_STREQ(message.c_str(), expectedSdp.c_str());
	};
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto connFixture = new rtc::RefCountedObject<PeerConnectionInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture, mockSendFunc);

	fixture->Test_SetPeerConnection(connFixture);

	SessionDescriptionInterfaceFixture sdp;

	EXPECT_CALL(sdp, ToString(_))
		.Times(Exactly(1))
		.WillOnce(Invoke([&](std::string* s) {
			s->assign(expectedContents);
			return true;
		}));

	// generates sdp offer
	fixture->OnSuccess(&sdp);

	// can assert sendfunc success
	ASSERT_TRUE(wasSendFuncCalled);
}

TEST(PeerConductorTests, PeerConductor_ICEGeneration_Success)
{
	auto expectedContents = std::string("test message contents");
	auto expectedIce = "{\n   \"candidate\" : \"" + expectedContents + "\",\n   \"sdpMLineIndex\" : 0,\n   \"sdpMid\" : \"\"\n}\n";
	auto wasSendFuncCalled = false;
	auto mockSendFunc = [&](const std::string& message)
	{
		wasSendFuncCalled = true;
		ASSERT_STREQ(message.c_str(), expectedIce.c_str());
	};
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture, mockSendFunc);

	IceCandidateInterfaceFixture ice;

	EXPECT_CALL(ice, ToString(_))
		.Times(Exactly(1))
		.WillOnce(Invoke([&](std::string* s) {
			s->assign(expectedContents);
			return true;
		}));

	// generates ice candidate msg
	fixture->OnIceCandidate(&ice);

	// can assert sendfunc success
	ASSERT_TRUE(wasSendFuncCalled);
}

TEST(PeerConductorTests, PeerConductor_HandleMessage_Success)
{
	auto expectedContents = std::string("candidate:4029998969 1 udp 41361151 40.69.184.50 50017 typ relay raddr 0.0.0.0 rport 0 generation 0 ufrag fKSq network-id 1 network-cost 50");
	auto expectedIce = "{\n   \"candidate\" : \"" + expectedContents + "\",\n   \"sdpMLineIndex\" : 0,\n   \"sdpMid\" : \"\"\n}\n";
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto connFixture = new rtc::RefCountedObject<PeerConnectionInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture);

	fixture->Test_SetPeerConnection(connFixture);

	EXPECT_CALL(*connFixture, AddIceCandidate(_))
		.Times(Exactly(1))
		.WillOnce(Return(true));

	// simulate handing msg
	ASSERT_TRUE(fixture->HandlePeerMessage(expectedIce));
}

TEST(PeerConductorTests, PeerConductor_HandleMessage_CreateOffer)
{
	auto expectedContents = std::string("v=0\no=- 4489647023841143573 2 IN IP4 127.0.0.1\ns=-\nt=0 0\n");
	auto expectedIce = "{\n   \"sdp\" : \"" + expectedContents + "\",\n   \"type\" : \"offer\",\n   \"sdpMid\" : \"\"\n}\n";
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto connFixture = new rtc::RefCountedObject<PeerConnectionInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture);

	fixture->Test_SetPeerConnection(connFixture);

	EXPECT_CALL(*connFixture, SetRemoteDescription(_, _))
		.Times(Exactly(1));
	EXPECT_CALL(*connFixture, CreateAnswer(_, _))
		.Times(Exactly(1));

	// simulate handing msg
	ASSERT_TRUE(fixture->HandlePeerMessage(expectedIce));
}

TEST(PeerConductorTests, PeerConductor_MultiPeer_Alloc_Success)
{
	auto factoryFixture = new rtc::RefCountedObject<PeerConnectionFactoryInterfaceFixture>();
	auto condFixture = new rtc::RefCountedObject<PeerConductorFixture>(factoryFixture);
	auto connFixture = new rtc::RefCountedObject<PeerConnectionInterfaceFixture>();
	auto fixture = new rtc::RefCountedObject<MultiPeerConductorFixture>(factoryFixture);

	condFixture->Test_SetPeerConnection(connFixture);

	EXPECT_CALL(*fixture, SafeAllocatePeerMapEntry_Counter())
		.Times(Exactly(3));

	fixture->ConnectToPeer(0);
	fixture->ConnectToPeer(1);
	fixture->ConnectToPeer(2);

	ASSERT_EQ(fixture->Peers().size(), 3);
}
