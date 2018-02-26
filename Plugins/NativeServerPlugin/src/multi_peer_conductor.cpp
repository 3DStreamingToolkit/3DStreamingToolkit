#include "pch.h"

#include "defaults.h"
#include "multi_peer_conductor.h"

MultiPeerConductor::MultiPeerConductor(shared_ptr<WebRTCConfig> config,
	ID3D11Device* d3d_device,
	bool enable_software_encoder) :
	webrtc_config_(config),
	d3d_device_(d3d_device),
	main_window_(nullptr)
{
	signalling_client_.RegisterObserver(this);
	peer_factory_ = webrtc::CreatePeerConnectionFactory();
	process_thread_ = rtc::Thread::Create();
	process_thread_->Start(this);
}

MultiPeerConductor::~MultiPeerConductor()
{
	process_thread_->Quit();
}

const map<int, scoped_refptr<DirectXPeerConductor>>& MultiPeerConductor::Peers() const
{
	return connected_peers_;
}

PeerConnectionClient& MultiPeerConductor::PeerConnection()
{
	return signalling_client_;
}

void MultiPeerConductor::ConnectSignallingAsync(const string& client_name)
{
	signalling_client_.Connect(webrtc_config_->server, webrtc_config_->port, client_name);
}

void MultiPeerConductor::SetMainWindow(MainWindow* main_window)
{
	main_window_ = main_window;
}

void MultiPeerConductor::SetDataChannelMessageHandler(const function<void(int, const string&)>& data_channel_handler)
{
	data_channel_handler_ = data_channel_handler;
}

void MultiPeerConductor::OnIceConnectionChange(int peer_id, PeerConnectionInterface::IceConnectionState new_state)
{
	// peer disconnected
	if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		connected_peers_.erase(peer_id);
	}
}

void MultiPeerConductor::HandleDataChannelMessage(int peer_id, const string& message)
{
	if (data_channel_handler_)
	{
		data_channel_handler_(peer_id, message);
	}
}

void MultiPeerConductor::OnSignedIn()
{
	should_process_queue_.store(true);
	if (main_window_ && main_window_->IsWindow())
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void MultiPeerConductor::OnDisconnected()
{
	should_process_queue_.store(false);
}

void MultiPeerConductor::OnPeerConnected(int id, const string& name)
{
	connected_peers_[id] = new RefCountedObject<DirectXPeerConductor>(id,
		name,
		webrtc_config_,
		peer_factory_,
		[&, id](const string& message)
		{
			message_queue_.push(MessageEntry(id, message));
			rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
		},
		d3d_device_.Get());

	connected_peers_[id]->SignalIceConnectionChange.connect(this, &MultiPeerConductor::OnIceConnectionChange);
	connected_peers_[id]->SignalDataChannelMessage.connect(this, &MultiPeerConductor::HandleDataChannelMessage);

	// Refresh the list if we're showing it.
	if (main_window_ && main_window_->IsWindow() && main_window_->current_ui() == MainWindow::LIST_PEERS)
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void MultiPeerConductor::OnPeerDisconnected(int peer_id)
{
	connected_peers_.erase(peer_id);
}

void MultiPeerConductor::OnMessageFromPeer(int peer_id, const string& message)
{
	connected_peers_[peer_id]->HandlePeerMessage(message);
}

void MultiPeerConductor::OnMessageSent(int err) {}

void MultiPeerConductor::OnServerConnectionFailure() {}

void MultiPeerConductor::OnMessage(Message* msg)
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

void MultiPeerConductor::Run(Thread* thread)
{
	while (!thread->IsQuitting())
	{
		thread->ProcessMessages(500);
	}
}

void MultiPeerConductor::StartLogin(const std::string& server, int port)
{
	signalling_client_.Connect(server, port, "renderingserver_" + GetPeerName());
}

void MultiPeerConductor::DisconnectFromServer() {}

void MultiPeerConductor::ConnectToPeer(int peer_id)
{
	auto it = connected_peers_.find(peer_id);
	if (it != connected_peers_.end())
	{
		DirectXPeerConductor* peer = it->second;
		if (!peer->IsConnected())
		{
			peer->AllocatePeerConnection(true);
		}
	}
}

void MultiPeerConductor::DisconnectFromCurrentPeer() {}

void MultiPeerConductor::UIThreadCallback(int msg_id, void* data) {}

void MultiPeerConductor::Close() {}
