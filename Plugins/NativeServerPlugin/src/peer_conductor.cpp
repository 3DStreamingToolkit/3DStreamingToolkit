#include "pch.h"

#include "peer_conductor.h"

namespace 
{
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
	shared_ptr<WebRTCConfig> webrtc_config,
	scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
	const function<void(const string&)>& send_func) :
	id_(id),
	name_(name),
	webrtc_config_(webrtc_config),
	peer_factory_(peer_factory),
	send_func_(send_func),
	view_(new PeerView())
{
}

PeerConductor::~PeerConductor()
{
	LOG(INFO) << "dtor";

	peer_connection_->Close();
	peer_connection_ = NULL;
	peer_streams_.clear();
}

// id, new_state, threadsafe per-instance 
signal2<int, PeerConnectionInterface::IceConnectionState, sigslot::multi_threaded_local> SignalIceConnectionChange;

// This callback transfers the ownership of the |desc|.
// TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
// around ownership.
void PeerConductor::OnSuccess(SessionDescriptionInterface* desc)
{
	peer_connection_->SetLocalDescription(
		DummySetSessionDescriptionObserver::Create(), desc);

	string sdp;
	if (desc->ToString(&sdp))
	{
		Json::StyledWriter writer;
		Json::Value jmessage;
		jmessage[kSessionDescriptionTypeName] = desc->type();
		jmessage[kSessionDescriptionSdpName] = sdp;

		string message = writer.write(jmessage);

		send_func_(message);
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

	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();

	string sdp;
	if (!candidate->ToString(&sdp))
	{
		LOG(LS_ERROR) << "Failed to serialize candidate";
		return;
	}

	jmessage[kCandidateSdpName] = sdp;

	string message = writer.write(jmessage);

	send_func_(message);
}

void PeerConductor::OnAddStream(
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}

void PeerConductor::OnRemoveStream(
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
	peer_streams_.erase(std::remove(peer_streams_.begin(), peer_streams_.end(), stream), peer_streams_.end());

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
			view_->lookAt = lookAt;
			view_->up = up;
			view_->eye = eye;
		}
	}

	// emit a signal that we've gotten a message
	SignalMessage.emit(message);
}

void PeerConductor::OnStateChange() {}

void PeerConductor::HandlePeerMessage(const string& message)
{
	// if we don't know this peer, add it
	if (peer_connection_.get() == nullptr)
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
		peer_connection_->SetRemoteDescription(
			DummySetSessionDescriptionObserver::Create(),
			session_description);

		if (session_description->type() == webrtc::SessionDescriptionInterface::kOffer)
		{
			peer_connection_->CreateAnswer(this, NULL);
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

		if (!peer_connection_->AddIceCandidate(candidate.get()))
		{
			LOG(WARNING) << "Failed to apply the received candidate";
			return;
		}

		LOG(INFO) << " Received candidate :" << message;
	}
}

const int PeerConductor::Id() const
{
	return id_;
}

const string& PeerConductor::Name() const
{
	return name_;
}

const vector<scoped_refptr<webrtc::MediaStreamInterface>> PeerConductor::Streams() const
{
	return peer_streams_;
}

shared_ptr<PeerView> PeerConductor::View()
{
	return view_;
}

void PeerConductor::AllocatePeerConnection()
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;

	if (!webrtc_config_->ice_configuration.empty())
	{
		if (webrtc_config_->ice_configuration == "relay")
		{
			webrtc::PeerConnectionInterface::IceServer turnServer;
			turnServer.uri = "";
			turnServer.username = "";
			turnServer.password = "";

			if (!webrtc_config_->turn_server.uri.empty())
			{
				turnServer.uri = webrtc_config_->turn_server.uri;
				turnServer.username = webrtc_config_->turn_server.username;
				turnServer.password = webrtc_config_->turn_server.password;
			}

			turnServer.tls_cert_policy = webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;
			config.type = webrtc::PeerConnectionInterface::kRelay;
			config.servers.push_back(turnServer);
		}
		else
		{
			if (webrtc_config_->ice_configuration == "stun")
			{
				webrtc::PeerConnectionInterface::IceServer stunServer;
				stunServer.uri = "";
				if (!webrtc_config_->stun_server.uri.empty())
				{
					stunServer.urls.push_back(webrtc_config_->stun_server.uri);
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

	auto peerConnection = peer_factory_->CreatePeerConnection(config, &constraints, NULL, NULL, this);

	// create a peer connection for this peer
	peer_connection_ = peerConnection;

	scoped_refptr<VideoTrackInterface> video_track(
		peer_factory_->CreateVideoTrack(
			kVideoLabel,
			peer_factory_->CreateVideoSource(
				AllocateVideoCapturer(),
				NULL)));

	rtc::scoped_refptr<webrtc::MediaStreamInterface> peerStream =
		peer_factory_->CreateLocalMediaStream(kStreamLabel);

	peerStream->AddTrack(video_track);
	if (!peerConnection->AddStream(peerStream))
	{
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}

	// create a peer stream for this peer
	peer_streams_.push_back(peerStream);
}
