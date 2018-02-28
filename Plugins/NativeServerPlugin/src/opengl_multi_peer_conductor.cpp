#include "pch.h"

#include "defaults.h"
#include "opengl_multi_peer_conductor.h"

OpenGLMultiPeerConductor::OpenGLMultiPeerConductor(shared_ptr<WebRTCConfig> config) :
	webrtc_config_(config),
	main_window_(nullptr)
{
	signalling_client_.RegisterObserver(this);
	peer_factory_ = webrtc::CreatePeerConnectionFactory();
	process_thread_ = rtc::Thread::Create();
	process_thread_->Start(this);
}

OpenGLMultiPeerConductor::~OpenGLMultiPeerConductor()
{
	process_thread_->Quit();
}

const map<int, scoped_refptr<OpenGLPeerConductor>>& OpenGLMultiPeerConductor::Peers() const
{
	return connected_peers_;
}

PeerConnectionClient& OpenGLMultiPeerConductor::PeerConnection()
{
	return signalling_client_;
}

void OpenGLMultiPeerConductor::ConnectSignallingAsync(const string& client_name)
{
	signalling_client_.Connect(webrtc_config_->server, webrtc_config_->port, client_name);
}

void OpenGLMultiPeerConductor::SetMainWindow(MainWindow* main_window)
{
	main_window_ = main_window;
}

void OpenGLMultiPeerConductor::SetDataChannelMessageHandler(const function<void(int, const string&)>& data_channel_handler)
{
	data_channel_handler_ = data_channel_handler;
}

void OpenGLMultiPeerConductor::OnIceConnectionChange(int peer_id, PeerConnectionInterface::IceConnectionState new_state)
{
	// peer disconnected
	if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		connected_peers_.erase(peer_id);
	}
}

void OpenGLMultiPeerConductor::HandleDataChannelMessage(int peer_id, const string& message)
{
	if (data_channel_handler_)
	{
		data_channel_handler_(peer_id, message);
	}
}

void OpenGLMultiPeerConductor::OnSignedIn()
{
	should_process_queue_.store(true);
	if (main_window_ && main_window_->IsWindow())
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void OpenGLMultiPeerConductor::OnDisconnected()
{
	should_process_queue_.store(false);
}

void OpenGLMultiPeerConductor::OnPeerConnected(int id, const string& name)
{
	connected_peers_[id] = new RefCountedObject<OpenGLPeerConductor>(id,
		name,
		webrtc_config_,
		peer_factory_,
		[&, id](const string& message)
		{
			message_queue_.push(MessageEntry(id, message));
			rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
		});

	connected_peers_[id]->SignalIceConnectionChange.connect(this, &OpenGLMultiPeerConductor::OnIceConnectionChange);
	connected_peers_[id]->SignalDataChannelMessage.connect(this, &OpenGLMultiPeerConductor::HandleDataChannelMessage);

	// Refresh the list if we're showing it.
	if (main_window_ && main_window_->IsWindow() && main_window_->current_ui() == MainWindow::LIST_PEERS)
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void OpenGLMultiPeerConductor::OnPeerDisconnected(int peer_id)
{
	connected_peers_.erase(peer_id);
}

void OpenGLMultiPeerConductor::OnMessageFromPeer(int peer_id, const string& message)
{
	connected_peers_[peer_id]->HandlePeerMessage(message);
}

void OpenGLMultiPeerConductor::OnMessageSent(int err) {}

void OpenGLMultiPeerConductor::OnServerConnectionFailure() {}

void OpenGLMultiPeerConductor::OnMessage(Message* msg)
{
	if (!should_process_queue_.load() ||
		message_queue_.size() == 0)
	{
		return;
	}

	auto peerMessage = message_queue_.front();

	if (signalling_client_.SendToPeer(peerMessage.peer, peerMessage.message))
	{
		message_queue_.pop();
	}

	if (message_queue_.size() > 0)
	{
		rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
	}
}

void OpenGLMultiPeerConductor::Run(Thread* thread)
{
	while (!thread->IsQuitting())
	{
		thread->ProcessMessages(500);
	}
}

void OpenGLMultiPeerConductor::StartLogin(const std::string& server, int port)
{
	signalling_client_.Connect(server, port, "renderingserver_" + GetPeerName());
}

void OpenGLMultiPeerConductor::DisconnectFromServer() {}

void OpenGLMultiPeerConductor::ConnectToPeer(int peer_id)
{
	auto it = connected_peers_.find(peer_id);
	if (it != connected_peers_.end())
	{
		OpenGLPeerConductor* peer = it->second;
		if (!peer->IsConnected())
		{
			peer->AllocatePeerConnection(true);
		}
	}
}

void OpenGLMultiPeerConductor::DisconnectFromCurrentPeer() {}

void OpenGLMultiPeerConductor::UIThreadCallback(int msg_id, void* data) {}

void OpenGLMultiPeerConductor::Close() {}
