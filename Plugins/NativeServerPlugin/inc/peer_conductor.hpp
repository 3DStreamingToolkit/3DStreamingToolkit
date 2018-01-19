#pragma once

#include "pch.h"

#include <string>
#include <vector>
#include <DirectXMath.h>

#include "buffer_capturer.h"
#include "directx_buffer_capturer.h"

// from ConfigParser
#include "structs.h"

#include "webrtc/base/sigslot.h"
#include "webrtc/base/json.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeconstraints.h"

using namespace std;
using namespace webrtc;
using namespace rtc;
using namespace StreamingToolkit;
using namespace sigslot;

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

class PeerView
{
public:

	// dirextx vectors must be 16byte aligned
	// so we override the operators to support
	// new-ing this class

	void* operator new(size_t i)
	{
		return _mm_malloc(i, 16);
	}

	void operator delete(void* p)
	{
		_mm_free(p);
	}

	bool IsValid()
	{
		return lookAt != 0 && up != 0 && eye != 0 &&
			!DirectX::XMVector3Equal(eye, DirectX::XMVectorZero());
	}

	DirectX::XMVECTORF32 lookAt;
	DirectX::XMVECTORF32 up;
	DirectX::XMVECTORF32 eye;
};

// Abstract PeerConductor
class PeerConductor : public PeerConnectionObserver,
	public CreateSessionDescriptionObserver,
	public DataChannelObserver
{
public:
	PeerConductor(int id,
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

	~PeerConductor()
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


	virtual void OnFailure(const string& error) override
	{
		LOG(INFO) << "ICE Failure: " << error;
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
		PeerConnectionInterface::IceConnectionState new_state) override
	{
		SignalIceConnectionChange.emit(Id(), new_state);
	}

	// Called any time the IceGatheringState changes.
	virtual void OnIceGatheringChange(
		PeerConnectionInterface::IceGatheringState new_state) override {}

	// A new ICE candidate has been gathered.
	virtual void OnIceCandidate(const IceCandidateInterface* candidate) override
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

		m_sendFunc(message);
	}

	void OnAddStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}

	void OnRemoveStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override
	{
		// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
		m_peerStreams.erase(std::remove(m_peerStreams.begin(), m_peerStreams.end(), stream), m_peerStreams.end());

		stream->Release();
	}

	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override
	{
		channel->RegisterObserver(this);
	}

	//  A data buffer was successfully received.
	virtual void OnMessage(const DataBuffer& buffer) override
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

	virtual void OnStateChange() override {}

	void HandlePeerMessage(const string& message)
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

	const int Id() const
	{
		return m_id;
	}

	const string& Name() const
	{
		return m_name;
	}

	const vector<scoped_refptr<webrtc::MediaStreamInterface>> Streams() const
	{
		return m_peerStreams;
	}

	shared_ptr<PeerView> View()
	{
		return m_view;
	}

protected:
	// Allocates a buffer capturer for a single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() = 0;

private:
	void AllocatePeerConnection()
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

	int m_id;
	string m_name;
	shared_ptr<WebRTCConfig> m_webrtcConfig;
	scoped_refptr<PeerConnectionFactoryInterface> m_peerFactory;
	function<void(const string&)> m_sendFunc;
	scoped_refptr<PeerConnectionInterface> m_peerConnection;
	shared_ptr<PeerView> m_view;
	vector<scoped_refptr<webrtc::MediaStreamInterface>> m_peerStreams;

	// Names used for a IceCandidate JSON object.
	const char* kCandidateSdpMidName = "sdpMid";
	const char* kCandidateSdpMlineIndexName = "sdpMLineIndex";
	const char* kCandidateSdpName = "candidate";

	// Names used for stream labels.
	const char* kAudioLabel = "audio_label";
	const char* kVideoLabel = "video_label";
	const char* kStreamLabel = "stream_label";

	// Names used for a SessionDescription JSON object.
	const char* kSessionDescriptionTypeName = "type";
	const char* kSessionDescriptionSdpName = "sdp";
};

// A PeerConductor using a DirectXBufferCapturer
class DirectXPeerConductor : public PeerConductor,
	public has_slots<>
{
public:
	DirectXPeerConductor(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtcConfig,
		scoped_refptr<PeerConnectionFactoryInterface> peerFactory,
		const function<void(const string&)>& sendFunc,
		ID3D11Device* d3dDevice,
		bool enableSoftware = false) : PeerConductor(
			id,
			name,
			webrtcConfig,
			peerFactory,
			sendFunc
		),
		m_d3dDevice(d3dDevice),
		m_enableSoftware(enableSoftware)
	{

	}

	void SendFrame(ID3D11Texture2D* frame_buffer)
	{
		for each (auto spy in m_spies)
		{
			spy->SendFrame(frame_buffer);
		}
	}

protected:
	// Provide the same buffer capturer for each single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() override
	{
		unique_ptr<DirectXBufferCapturer> ownedPtr(new DirectXBufferCapturer(m_d3dDevice));

		ownedPtr->Initialize();

		if (m_enableSoftware)
		{
			ownedPtr->EnableSoftwareEncoder();
		}

		// rely on the deleter signal to let us know when it's gone and we should stop updating it
		ownedPtr->SignalDestroyed.connect(this, &DirectXPeerConductor::OnAllocatedPtrDeleted);

		// spy on this pointer. this only works because webrtc doesn't std::move it
		m_spies.push_back(ownedPtr.get());

		return ownedPtr;
	}

private:
	ID3D11Device* m_d3dDevice;
	bool m_enableSoftware;
	vector<DirectXBufferCapturer*> m_spies;

	void OnAllocatedPtrDeleted(BufferCapturer* ptr)
	{
		// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
		m_spies.erase(std::remove(m_spies.begin(), m_spies.end(), ptr), m_spies.end());
	}
};