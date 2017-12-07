#include <map>
#include <string>

#include "peer_connection_client.h"
#include "buffer_capturer.h"

#include "structs.h"

#include "webrtc/base/json.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeconstraints.h"

using namespace StreamingToolkit;

using namespace std;
using namespace rtc;
using namespace webrtc;

class DummySetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
public:
	static DummySetSessionDescriptionObserver* Create()
	{
		return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
	}

	virtual void OnSuccess()
	{
		LOG(INFO) << __FUNCTION__;
	}

	virtual void OnFailure(const std::string& error)
	{
		LOG(INFO) << __FUNCTION__ << " " << error;
	}

protected:
	DummySetSessionDescriptionObserver() {}
	~DummySetSessionDescriptionObserver() {}
};

class PeerBoundCreateSessionDescriptionObserver : public CreateSessionDescriptionObserver
{
public:
	PeerBoundCreateSessionDescriptionObserver(scoped_refptr<PeerConnectionInterface> peerConnection,
		const std::function<void(const string& message)>& sendFunc) :
		m_peerConnection(peerConnection),
		m_sendFunc(sendFunc)
	{}

	// This callback transfers the ownership of the |desc|.
	// TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
	// around ownership.
	virtual void OnSuccess(SessionDescriptionInterface* desc) override
	{
		m_peerConnection->SetLocalDescription(
			DummySetSessionDescriptionObserver::Create(), desc);

		string sdp;
		if (desc->ToString(&sdp))
		{
			Json::StyledWriter writer;
			Json::Value jmessage;
			jmessage[kSessionDescriptionTypeName] = desc->type();
			jmessage[kSessionDescriptionSdpName] = sdp;

			string message = writer.write(jmessage);

			m_sendFunc(message);
		}
	}

	virtual void OnFailure(const string& error) override {}

private:
	scoped_refptr<PeerConnectionInterface> m_peerConnection;
	std::function<void(const string& message)> m_sendFunc;

	// Names used for a SessionDescription JSON object.
	const char* kSessionDescriptionTypeName = "type";
	const char* kSessionDescriptionSdpName = "sdp";
};

class MultiPeerConductor : public PeerConnectionObserver,
	public PeerConnectionClientObserver
{
public:
	MultiPeerConductor(shared_ptr<WebRTCConfig> config,
		shared_ptr<BufferCapturer> bufferCapturer) :
		m_webrtcConfig(config),
		m_bufferCapturer(bufferCapturer),
		m_signallingClient("")
	{
		m_signallingClient.RegisterObserver(this);
		m_peerFactory = webrtc::CreatePeerConnectionFactory();
	}

	// Connect the signalling implementation to the signalling server
	void ConnectSignallingAsync(const string& clientName)
	{
		m_signallingClient.Connect(m_webrtcConfig->server, m_webrtcConfig->port, clientName);
	}

	// Triggered when the SignalingState changed.
	virtual void OnSignalingChange(
		PeerConnectionInterface::SignalingState new_state) override {}

	// Triggered when renegotiation is needed. For example, an ICE restart
	// has begun.
	virtual void OnRenegotiationNeeded() override {}

	// Called any time the IceConnectionState changes.
	//
	// Note that our ICE states lag behind the standard slightly. The most
	// notable differences include the fact that "failed" occurs after 15
	// seconds, not 30, and this actually represents a combination ICE + DTLS
	// state, so it may be "failed" if DTLS fails while ICE succeeds.
	virtual void OnIceConnectionChange(
		PeerConnectionInterface::IceConnectionState new_state) override {}

	// Called any time the IceGatheringState changes.
	virtual void OnIceGatheringChange(
		PeerConnectionInterface::IceGatheringState new_state) override {}

	// A new ICE candidate has been gathered.
	virtual void OnIceCandidate(const IceCandidateInterface* candidate) override {}

	void OnAddStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}

	void OnRemoveStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}

	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}

	
	virtual void OnSignedIn() override {}  // Called when we're logged on.

	virtual void OnDisconnected() override {}

	virtual void OnPeerConnected(int id, const string& name) override
	{
		m_connectedPeers[id] = name;
	}

	virtual void OnPeerDisconnected(int peer_id) override
	{
		m_connectedPeers.erase(peer_id);
	}

	virtual void OnMessageFromPeer(int peer_id, const string& message) override
	{
		auto peerConnection = m_peerConnections[peer_id];

		// if we don't know this peer, add it
		if (peerConnection.get() == nullptr)
		{
			// this is just the init that sets the m_peerConnections[] value
			OnPeerWebrtcConnected(peer_id);

			peerConnection = m_peerConnections[peer_id];
		}

		Json::Reader reader;
		Json::Value jmessage;
		if (!reader.parse(message, jmessage))
		{
			LOG(WARNING) << "Received unknown message. " << message;
			return;
		}

		string type;
		string json_object;

		rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);
		if (!type.empty())
		{
			if (type == "offer-loopback")
			{
				//TODO(bengreenier): reimplement
				return;
			}

			std::string sdp;
			if (!rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
			{
				LOG(WARNING) << "Can't parse received session description message.";
				return;
			}

			webrtc::SdpParseError error;
			webrtc::SessionDescriptionInterface* session_description(
				webrtc::CreateSessionDescription(type, sdp, &error));

			if (!session_description)
			{
				LOG(WARNING) << "Can't parse received session description message. "
					<< "SdpParseError was: " << error.description;

				return;
			}

			LOG(INFO) << " Received session description :" << message;
			peerConnection->SetRemoteDescription(
				DummySetSessionDescriptionObserver::Create(),
				session_description);

			if (session_description->type() == webrtc::SessionDescriptionInterface::kOffer)
			{
				// TODO(bengreenier): will this auto cleanup?
				scoped_refptr<PeerBoundCreateSessionDescriptionObserver> pcListener(
					new RefCountedObject<PeerBoundCreateSessionDescriptionObserver>(
						peerConnection,
						[&, peer_id](const string& message)
					{
						m_signallingClient.SendToPeer(peer_id, message);
					})
				);

				// TODO(bengreenier): the handler here needs to be aware of which pc this is
				peerConnection->CreateAnswer(pcListener, NULL);
			}

			return;
		}
		else
		{
			std::string sdp_mid;
			int sdp_mlineindex = 0;
			std::string sdp;
			if (!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) ||
				!rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) ||
				!rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
			{
				LOG(WARNING) << "Can't parse received message.";
				return;
			}

			webrtc::SdpParseError error;
			std::unique_ptr<webrtc::IceCandidateInterface> candidate(
				webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));

			if (!candidate.get())
			{
				LOG(WARNING) << "Can't parse received candidate message. "
					<< "SdpParseError was: " << error.description;

				return;
			}

			if (!peerConnection->AddIceCandidate(candidate.get()))
			{
				LOG(WARNING) << "Failed to apply the received candidate";
				return;
			}

			LOG(INFO) << " Received candidate :" << message;
		}
	}

	virtual void OnMessageSent(int err) override {}

	virtual void OnServerConnectionFailure() override {}

	void OnPeerWebrtcConnected(int id)
	{
		webrtc::PeerConnectionInterface::RTCConfiguration config;

		if (!m_webrtcConfig->ice_configuration.empty())
		{
			if (m_webrtcConfig->ice_configuration == "relay")
			{
				webrtc::PeerConnectionInterface::IceServer turnServer;
				turnServer.uri = "";
				turnServer.username = "";
				turnServer.password = "";

				if (!m_webrtcConfig->turn_server.uri.empty())
				{
					turnServer.uri = m_webrtcConfig->turn_server.uri;
					turnServer.username = m_webrtcConfig->turn_server.username;
					turnServer.password = m_webrtcConfig->turn_server.password;
				}

				turnServer.tls_cert_policy = webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;
				config.type = webrtc::PeerConnectionInterface::kRelay;
				config.servers.push_back(turnServer);
			}
			else
			{
				if (m_webrtcConfig->ice_configuration == "stun")
				{
					webrtc::PeerConnectionInterface::IceServer stunServer;
					stunServer.uri = "";
					if (!m_webrtcConfig->stun_server.uri.empty())
					{
						stunServer.urls.push_back(m_webrtcConfig->stun_server.uri);
						config.servers.push_back(stunServer);
					}
				}
				else
				{
					webrtc::PeerConnectionInterface::IceServer stunServer;

					stunServer.urls.push_back("stun:stun.l.google.com:19302");
					config.servers.push_back(stunServer);
				}
			}
		}

		webrtc::FakeConstraints constraints;

		// TODO(bengreenier): make optional again for loopback
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

		auto peerConnection = m_peerFactory->CreatePeerConnection(config, &constraints, NULL, NULL, this);

		// create a peer connection for this peer
		m_peerConnections[id] = peerConnection;

		scoped_refptr<VideoTrackInterface> video_track(
			m_peerFactory->CreateVideoTrack(
				kVideoLabel,
				m_peerFactory->CreateVideoSource(
					std::unique_ptr<cricket::VideoCapturer>(m_bufferCapturer.get()),
					NULL)));

		rtc::scoped_refptr<webrtc::MediaStreamInterface> peerStream =
			m_peerFactory->CreateLocalMediaStream(kStreamLabel);

		peerStream->AddTrack(video_track);
		if (!peerConnection->AddStream(peerStream))
		{
			LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
		}

		// create a peer stream for this peer
		m_peerStreams[id] = peerStream;
	}

	void OnPeerWebrtcDisconnect(int id)
	{
		m_peerStreams.erase(id);
		m_peerConnections.erase(id);
	}

private:
	PeerConnectionClient m_signallingClient;
	shared_ptr<WebRTCConfig> m_webrtcConfig;
	shared_ptr<BufferCapturer> m_bufferCapturer;
	scoped_refptr<PeerConnectionFactoryInterface> m_peerFactory;
	map<int, string> m_connectedPeers;
	map<int, scoped_refptr<PeerConnectionInterface>> m_peerConnections;
	map<int, scoped_refptr<webrtc::MediaStreamInterface>> m_peerStreams;

	const char* kAudioLabel = "audio_label";
	const char* kVideoLabel = "video_label";
	const char* kStreamLabel = "stream_label";

	// Names used for a IceCandidate JSON object.
	const char* kCandidateSdpMidName = "sdpMid";
	const char* kCandidateSdpMlineIndexName = "sdpMLineIndex";
	const char* kCandidateSdpName = "candidate";

	// Names used for a SessionDescription JSON object.
	const char* kSessionDescriptionTypeName = "type";
	const char* kSessionDescriptionSdpName = "sdp";

	// Names used for data channels
	const char* kInputDataChannelName = "inputDataChannel";
};