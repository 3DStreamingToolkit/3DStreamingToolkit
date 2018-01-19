#include "pch.h"
#include "peer_conductor.h"

namespace {
	// Mock (does nothing) SetSessionDescriptionObserver
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
}

PeerConductor::PeerConductor(int id,
	const string& name,
	shared_ptr<WebRTCConfig> webrtcConfig,
	scoped_refptr<PeerConnectionFactoryInterface> peerFactory,
	const function<void(const string&)>& sendFunc) :
	m_id(id),
	m_name(name),
	m_webrtcConfig(webrtcConfig),
	m_peerFactory(peerFactory),
	m_sendFunc(sendFunc),
	m_view(new PeerView())
{

}

PeerConductor::~PeerConductor()
{
	LOG(INFO) << "dtor";

	m_peerConnection->Close();
	m_peerConnection = NULL;
	m_peerStreams.clear();
}

// m_id, new_state, threadsafe per-instance 
signal2<int, PeerConnectionInterface::IceConnectionState, sigslot::multi_threaded_local> SignalIceConnectionChange;

// This callback transfers the ownership of the |desc|.
// TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
// around ownership.
void PeerConductor::OnSuccess(SessionDescriptionInterface* desc)
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


void PeerConductor::OnFailure(const string& error)
{
	LOG(INFO) << "ICE Failure: " << error;
}

void PeerConductor::OnSignalingChange(
	PeerConnectionInterface::SignalingState new_state) {}

void PeerConductor::OnRenegotiationNeeded() {}

void PeerConductor::OnIceConnectionChange(
	PeerConnectionInterface::IceConnectionState new_state)
{
	SignalIceConnectionChange.emit(Id(), new_state);
}

void PeerConductor::OnIceGatheringChange(
	PeerConnectionInterface::IceGatheringState new_state) {}

void PeerConductor::OnIceCandidate(const IceCandidateInterface* candidate)
{
	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage[PeerConductor::kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[PeerConductor::kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();

	string sdp;
	if (!candidate->ToString(&sdp))
	{
		LOG(LS_ERROR) << "Failed to serialize candidate";
		return;
	}

	jmessage[kCandidateSdpName] = sdp;

	string message = writer.write(jmessage);

	m_sendFunc(message);
}

void PeerConductor::OnAddStream(
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

void PeerConductor::OnRemoveStream(
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
	m_peerStreams.erase(std::remove(m_peerStreams.begin(), m_peerStreams.end(), stream), m_peerStreams.end());

	stream->Release();
}

void PeerConductor::OnDataChannel(
	rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
	channel->RegisterObserver(this);
}

void PeerConductor::OnMessage(const DataBuffer& buffer)
{
	std::string message((const char*)buffer.data.data(), buffer.data.size());

	char type[256];
	char body[1024];
	Json::Reader reader;
	Json::Value msg = NULL;
	reader.parse(message, msg, false);

	if (msg.isMember("type") && msg.isMember("body"))
	{
		strcpy(type, msg.get("type", "").asCString());
		strcpy(body, msg.get("body", "").asCString());
		std::istringstream datastream(body);
		std::string token;

		if (strcmp(type, "camera-transform-lookat") == 0)
		{
			// Eye point.
			getline(datastream, token, ',');
			float eyeX = stof(token);
			getline(datastream, token, ',');
			float eyeY = stof(token);
			getline(datastream, token, ',');
			float eyeZ = stof(token);

			// Focus point.
			getline(datastream, token, ',');
			float focusX = stof(token);
			getline(datastream, token, ',');
			float focusY = stof(token);
			getline(datastream, token, ',');
			float focusZ = stof(token);

			// Up vector.
			getline(datastream, token, ',');
			float upX = stof(token);
			getline(datastream, token, ',');
			float upY = stof(token);
			getline(datastream, token, ',');
			float upZ = stof(token);

			const DirectX::XMVECTORF32 lookAt = { focusX, focusY, focusZ, 0.f };
			const DirectX::XMVECTORF32 up = { upX, upY, upZ, 0.f };
			const DirectX::XMVECTORF32 eye = { eyeX, eyeY, eyeZ, 0.f };

			// update the view
			m_view->lookAt = lookAt;
			m_view->up = up;
			m_view->eye = eye;
		}
	}

	// TODO(bengreenier): we can raise a signal with this data to allow for further parsing (not our responsibility)
}

void PeerConductor::OnStateChange() {}

void PeerConductor::HandlePeerMessage(const string& message)
{
	// if we don't know this peer, add it
	if (m_peerConnection.get() == nullptr)
	{
		// this is just the init that sets the m_peerConnection value
		AllocatePeerConnection();
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
		m_peerConnection->SetRemoteDescription(
			DummySetSessionDescriptionObserver::Create(),
			session_description);

		if (session_description->type() == webrtc::SessionDescriptionInterface::kOffer)
		{
			m_peerConnection->CreateAnswer(this, NULL);
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

		if (!m_peerConnection->AddIceCandidate(candidate.get()))
		{
			LOG(WARNING) << "Failed to apply the received candidate";
			return;
		}

		LOG(INFO) << " Received candidate :" << message;
	}
}

const int PeerConductor::Id() const
{
	return m_id;
}

const string& PeerConductor::Name() const
{
	return m_name;
}

const vector<scoped_refptr<webrtc::MediaStreamInterface>> PeerConductor::Streams() const
{
	return m_peerStreams;
}

shared_ptr<PeerView> PeerConductor::View()
{
	return m_view;
}

void PeerConductor::AllocatePeerConnection()
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
	m_peerConnection = peerConnection;

	scoped_refptr<VideoTrackInterface> video_track(
		m_peerFactory->CreateVideoTrack(
			kVideoLabel,
			m_peerFactory->CreateVideoSource(
				AllocateVideoCapturer(),
				NULL)));

	rtc::scoped_refptr<webrtc::MediaStreamInterface> peerStream =
		m_peerFactory->CreateLocalMediaStream(kStreamLabel);

	peerStream->AddTrack(video_track);
	if (!peerConnection->AddStream(peerStream))
	{
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}

	// create a peer stream for this peer
	m_peerStreams.push_back(peerStream);
}