/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pch.h"

#include <memory>
#include <utility>
#include <vector>
#include <fstream>
#include <d3d11_4.h>
#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>

#include "conductor.h"
#include "defaults.h"
#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"

#include "plugindefs.h"
#include "buffer_capturer.h"

using namespace StreamingToolkit;
using namespace Microsoft::WRL;

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

// Names used for data channels
const char kInputDataChannelName[] = "inputDataChannel";

#define DTLS_ON  true
#define DTLS_OFF false

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

Conductor::Conductor(
	PeerConnectionClient* client,
	BufferCapturer* buffer_capturer,
	MainWindow* main_window,
	WebRTCConfig* webrtc_config,
	PeerConnectionObserver* connection_observer) :
		peer_id_(-1),
		loopback_(false),
		is_closing_(false),
		client_(client),
		buffer_capturer_(buffer_capturer),
		main_window_(main_window),
		webrtc_config_(webrtc_config),
		input_data_handler_(nullptr)
{
	client_->RegisterObserver(this);

	if (main_window_->IsWindow())
	{
		thread_callback_ = new ThreadSafeMainWindowCallback(this);
		main_window->RegisterObserver(thread_callback_);
	}

	if (connection_observer != nullptr)
	{
		client_observer_ = new PeerConnectionMultiObserver({ this, connection_observer });
	}
	else
	{
		// we technically don't need the multi observer here, but it makes cleanup easier
		// as we can just always delete client_observer_ rather than needing a conditional
		// and the overhead is low.
		client_observer_ = new PeerConnectionMultiObserver({ this });
	}
}

Conductor::~Conductor() 
{
	RTC_DCHECK(peer_connection_.get() == NULL);

	// cleanup the observer
	delete client_observer_;

	// cleanup the thread callback
	delete thread_callback_;
}

bool Conductor::connection_active() const
{
	return peer_connection_.get() != NULL;
}

bool Conductor::is_closing() const
{
	return is_closing_;
}

void Conductor::SetTurnCredentials(const std::string& username, const std::string& password)
{
	turn_username_ = username;
	turn_password_ = password;
}

void Conductor::SetInputDataHandler(InputDataHandler* handler)
{
	input_data_handler_ = handler;
}

void Conductor::Close() 
{
	is_closing_ = true;
	client_->SignOut();
	client_->Shutdown();

	if (peer_connection_ != nullptr)
	{
		peer_connection_->Close();
	}
	
	if (data_channel_ != nullptr)
	{
		data_channel_->Close();
	}

	bool isopen = false;

	if (data_channel_observer_ != nullptr)
	{
		isopen = data_channel_observer_->IsOpen();
	}

	DeletePeerConnection();
}

bool Conductor::InitializePeerConnection()
{
	RTC_DCHECK(peer_connection_factory_.get() == NULL);
	RTC_DCHECK(peer_connection_.get() == NULL);

	peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();

	if (!peer_connection_factory_.get())
	{
		if (main_window_->IsWindow())
		{
			main_window_->MessageBox(
				"Error",
				"Failed to initialize PeerConnectionFactory",
				true);
		}

		DeletePeerConnection();
		return false;
	}

	if (!CreatePeerConnection(DTLS_ON)) 
	{
		if (main_window_->IsWindow())
		{
			main_window_->MessageBox("Error", "CreatePeerConnection failed", true);
		}

		DeletePeerConnection();
	}

	AddStreams();
	return peer_connection_.get() != NULL;
}

bool Conductor::ReinitializePeerConnectionForLoopback()
{
	loopback_ = true;
	rtc::scoped_refptr<webrtc::StreamCollectionInterface> streams(
		peer_connection_->local_streams());

	peer_connection_ = NULL;
	if (CreatePeerConnection(DTLS_OFF))
	{
		for (size_t i = 0; i < streams->count(); ++i)
		{
			peer_connection_->AddStream(streams->at(i));
		}

		peer_connection_->CreateOffer(this, NULL);
	}

	return peer_connection_.get() != NULL;
}

bool Conductor::CreatePeerConnection(bool dtls)
{
	RTC_DCHECK(peer_connection_factory_.get() != NULL);
	RTC_DCHECK(peer_connection_.get() == NULL);

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

			// if we're given explicit turn creds at runtime, steamroll any config values
			if (!turn_username_.empty() && !turn_password_.empty())
			{
				turnServer.username = turn_username_;
				turnServer.password = turn_password_;
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
				stunServer.urls.push_back(GetPeerConnectionString());
				config.servers.push_back(stunServer);
			}
		}
	}

	webrtc::FakeConstraints constraints;
	if (dtls)
	{
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");
	}
	else
	{
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "false");
	}

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(
		config, &constraints, NULL, NULL, client_observer_);

	return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection()
{
	peer_connection_ = NULL;
	active_streams_.clear();

	if (main_window_->IsWindow())
	{
		main_window_->StopLocalRenderer();
		main_window_->StopRemoteRenderer();
	}

	peer_connection_factory_ = NULL;
	peer_id_ = -1;
	loopback_ = false;
}

void Conductor::EnsureStreamingUI()
{
	RTC_DCHECK(peer_connection_.get() != NULL);
	if (main_window_->IsWindow() && main_window_->current_ui() != MainWindow::STREAMING)
	{
		main_window_->SwitchToStreamingUI();
	}
}

//-------------------------------------------------------------------------
// PeerConnectionObserver implementation.
//-------------------------------------------------------------------------

// Called when a remote stream is added
void Conductor::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	LOG(INFO) << __FUNCTION__ << " " << stream->label();
	if (main_window_->IsWindow())
	{
		main_window_->QueueUIThreadCallback(NEW_STREAM_ADDED, stream.release());
	}
	else
	{
		NewStreamAdded(stream.release());
	}
}

// Called when a remote stream is removed
void Conductor::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	LOG(INFO) << __FUNCTION__ << " " << stream->label();
	if (main_window_->IsWindow())
	{
		main_window_->QueueUIThreadCallback(STREAM_REMOVED, stream.release());
	}
	else
	{
		StreamRemoved(stream.release());
	}
}

void Conductor::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
	data_channel_ = channel;
	data_channel_observer_.reset(new InputDataChannelObserver(channel, input_data_handler_));
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

	// For loopback test. To save some connecting delay.
	if (loopback_)
	{
		if (!peer_connection_->AddIceCandidate(candidate))
		{
			LOG(WARNING) << "Failed to apply the received candidate";
		}

		return;
	}

	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	std::string sdp;
	if (!candidate->ToString(&sdp))
	{
		LOG(LS_ERROR) << "Failed to serialize candidate";
		return;
	}

	jmessage[kCandidateSdpName] = sdp;
	SendMessage(writer.write(jmessage));
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn()
{
	LOG(INFO) << __FUNCTION__;
	if (main_window_->IsWindow())
	{
		main_window_->SwitchToPeerList(client_->peers());
	}
}

void Conductor::OnDisconnected()
{
	LOG(INFO) << __FUNCTION__;

	DeletePeerConnection();
	if (main_window_->IsWindow())
	{
		main_window_->SwitchToConnectUI();
	}
}

void Conductor::OnPeerConnected(int id, const std::string& name)
{
	LOG(INFO) << __FUNCTION__;

	// Refresh the list if we're showing it.
	if (main_window_->IsWindow() && main_window_->current_ui() == MainWindow::LIST_PEERS)
	{
		main_window_->SwitchToPeerList(client_->peers());
	}
}

void Conductor::OnPeerDisconnected(int id)
{
	LOG(INFO) << __FUNCTION__;
	if (main_window_->IsWindow())
	{
		if (id == peer_id_)
		{
			LOG(INFO) << "Our peer disconnected";
			main_window_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
		}
		else if (main_window_->current_ui() == MainWindow::LIST_PEERS)
		{
			// Refresh the list if we're showing it.
			main_window_->SwitchToPeerList(client_->peers());
		}
	}
	else if (id == peer_id_)
	{
		LOG(INFO) << "PEER_CONNECTION_CLOSED";
		DeletePeerConnection();
		DisconnectFromServer();
	}
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message)
{
	RTC_DCHECK(peer_id_ == peer_id || peer_id_ == -1);
	RTC_DCHECK(!message.empty());

	if (!peer_connection_.get()) 
	{
		RTC_DCHECK(peer_id_ == -1);
		peer_id_ = peer_id;

		if (!InitializePeerConnection()) 
		{
			LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
			client_->SignOut();
			return;
		}
	}
	else if (peer_id != peer_id_)
	{
		RTC_DCHECK(peer_id_ != -1);
		LOG(WARNING) << "Received a message from unknown peer while already in a "
						"conversation with a different peer.";
		return;
	}

	Json::Reader reader;
	Json::Value jmessage;
	if (!reader.parse(message, jmessage))
	{
		LOG(WARNING) << "Received unknown message. " << message;
		return;
	}

	std::string type;
	std::string json_object;

	rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);
	if (!type.empty()) 
	{
		if (type == "offer-loopback")
		{
			// This is a loopback call.
			// Recreate the peerconnection with DTLS disabled.
			if (!ReinitializePeerConnectionForLoopback())
			{
				LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
				DeletePeerConnection();
				client_->SignOut();
			}

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
		return;
	}
}

void Conductor::OnMessageSent(int err)
{
	// Process the next pending message if any.
	if (main_window_->IsWindow())
	{
		main_window_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
	}
}

void Conductor::OnServerConnectionFailure()
{
	if (main_window_->IsWindow())
	{
		main_window_->MessageBox("Error", ("Failed to connect to " + server_).c_str(), true);
	}
}

//-------------------------------------------------------------------------
// MainWndCallback implementation.
//-------------------------------------------------------------------------

void Conductor::StartLogin(const std::string& server, int port)
{
	if (client_->is_connected())
	{
		return;
	}

	server_ = server;
	client_->Connect(server, port, "renderingserver_" + GetPeerName());
}

void Conductor::DisconnectFromServer()
{
	if (client_->is_connected())
	{
		client_->SignOut();
	}
}

void Conductor::ConnectToPeer(int peer_id) 
{
	RTC_DCHECK(peer_id_ == -1);
	RTC_DCHECK(peer_id != -1);

	if (peer_connection_.get())
	{
		if (main_window_->IsWindow())
		{
			main_window_->MessageBox(
				"Error",
				"We only support connecting to one peer at a time",
				true);
		}

		return;
	}

	if (InitializePeerConnection())
	{
		peer_id_ = peer_id;
		webrtc::DataChannelInit config;
		config.ordered = false;
		config.maxRetransmits = 0;
		data_channel_ = peer_connection_->CreateDataChannel(kInputDataChannelName, &config);
		data_channel_observer_.reset(new InputDataChannelObserver(data_channel_, input_data_handler_));
		peer_connection_->CreateOffer(this, NULL);
	}
	else if (main_window_->IsWindow())
	{
		main_window_->MessageBox("Error", "Failed to initialize PeerConnection", true);
	}
}

std::unique_ptr<cricket::VideoCapturer> Conductor::OpenVideoCaptureDevice()
{
	std::unique_ptr<cricket::VideoCapturer> capturer(buffer_capturer_);
	return capturer;
}

void Conductor::AddStreams()
{
	if (active_streams_.find(kStreamLabel) != active_streams_.end())
	{
		return;  // Already added.
	}

	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
		peer_connection_factory_->CreateVideoTrack(
			kVideoLabel,
			peer_connection_factory_->CreateVideoSource(
				OpenVideoCaptureDevice(),
				NULL)));

	if (main_window_->IsWindow())
	{
		main_window_->StartLocalRenderer(video_track);
	}

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
		peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

	stream->AddTrack(video_track);
	if (!peer_connection_->AddStream(stream))
	{
		LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}

	typedef std::pair<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface>>
		MediaStreamPair;

	active_streams_.insert(MediaStreamPair(stream->label(), stream));
	if (main_window_->IsWindow())
	{
		main_window_->SwitchToStreamingUI();
	}
}

void Conductor::DisconnectFromCurrentPeer()
{
	LOG(INFO) << __FUNCTION__;
	if (peer_connection_.get())
	{
		client_->SendHangUp(peer_id_);
		DeletePeerConnection();
	}

	if (main_window_->IsWindow())
	{
		main_window_->SwitchToPeerList(client_->peers());
	}
}

void Conductor::UIThreadCallback(int msg_id, void* data)
{
	switch (msg_id)
	{
		case PEER_CONNECTION_CLOSED:
			LOG(INFO) << "PEER_CONNECTION_CLOSED";
			DeletePeerConnection();

			RTC_DCHECK(active_streams_.empty());
			if (client_->is_connected())
			{
				main_window_->SwitchToPeerList(client_->peers());
			}
			else
			{
				main_window_->SwitchToConnectUI();
			}

			break;

		case SEND_MESSAGE_TO_PEER:
		{
			std::string* msg = reinterpret_cast<std::string*>(data);
			SendMessageToPeer(msg);
			break;
		}

		case NEW_STREAM_ADDED:
		{
			webrtc::MediaStreamInterface* stream = 
				reinterpret_cast<webrtc::MediaStreamInterface*>(data);

			NewStreamAdded(stream);
			break;
		}

		case STREAM_REMOVED:
		{
			webrtc::MediaStreamInterface* stream =
				reinterpret_cast<webrtc::MediaStreamInterface*>(data);

			StreamRemoved(stream);
			break;
		}

		default:
			RTC_NOTREACHED();
			break;
	}
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
	peer_connection_->SetLocalDescription(
		DummySetSessionDescriptionObserver::Create(), desc);

	std::string sdp;
	desc->ToString(&sdp);

	// For loopback test. To save some connecting delay.
	if (loopback_)
	{
		// Replace message type from "offer" to "answer"
		webrtc::SessionDescriptionInterface* session_description(
			webrtc::CreateSessionDescription("answer", sdp, nullptr));

		peer_connection_->SetRemoteDescription(
			DummySetSessionDescriptionObserver::Create(), session_description);

		return;
	}

	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage[kSessionDescriptionTypeName] = desc->type();
	jmessage[kSessionDescriptionSdpName] = sdp;
	SendMessage(writer.write(jmessage));
}

void Conductor::OnFailure(const std::string& error)
{
	LOG(LERROR) << error;
}

void Conductor::SendMessage(const std::string& json_object)
{
	std::string* msg = new std::string(json_object);
	/*if (main_window_->IsWindow())
	{
		main_window_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
	}
	else
	{*/
		SendMessageToPeer(msg);
	//}
}

void Conductor::NewStreamAdded(webrtc::MediaStreamInterface* stream)
{
	webrtc::VideoTrackVector tracks = stream->GetVideoTracks();

	// Only render the first track.
	if (!tracks.empty() && main_window_->IsWindow())
	{
		webrtc::VideoTrackInterface* track = tracks[0];
		main_window_->StartRemoteRenderer(track);
	}

	stream->Release();
}

void Conductor::StreamRemoved(webrtc::MediaStreamInterface* stream)
{
	// Remote peer stopped sending a stream.
	stream->Release();
}

void Conductor::SendMessageToPeer(std::string* msg)
{
	LOG(INFO) << "SEND_MESSAGE_TO_PEER";

	if (msg)
	{
		// For convenience, we always run the message through the queue.
		// This way we can be sure that messages are sent to the server
		// in the same order they were signaled without much hassle.
		pending_messages_.push_back(msg);
	}

	if (!pending_messages_.empty() && !client_->IsSendingMessage())
	{
		msg = pending_messages_.front();
		pending_messages_.pop_front();

		if (!client_->SendToPeer(peer_id_, *msg) && peer_id_ != -1)
		{
			LOG(LS_ERROR) << "SendToPeer failed";
			DisconnectFromServer();
		}

		delete msg;
	}

	if (!peer_connection_.get())
	{
		peer_id_ = -1;
	}
}
