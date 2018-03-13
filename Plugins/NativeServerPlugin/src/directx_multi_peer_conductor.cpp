#include "pch.h"

#include "defaults.h"
#include "directx_multi_peer_conductor.h"

DirectXMultiPeerConductor::DirectXMultiPeerConductor(shared_ptr<FullServerConfig> config,
	ID3D11Device* d3d_device) :
	config_(config),
	d3d_device_(d3d_device),
	main_window_(nullptr),
	max_capacity_(-1),
	cur_capacity_(-1)
{
	signalling_client_.RegisterObserver(this);
	signalling_client_.SignalConnected.connect(this, &DirectXMultiPeerConductor::HandleSignalConnect);

	if (config_->webrtc_config->heartbeat > 0)
	{
		signalling_client_.SetHeartbeatMs(config_->webrtc_config->heartbeat);
	}

	if (config_->server_config->server_config.system_capacity > 0)
	{
		max_capacity_ = config_->server_config->server_config.system_capacity;
		cur_capacity_ = max_capacity_;
	}

	peer_factory_ = webrtc::CreatePeerConnectionFactory();
	process_thread_ = rtc::Thread::Create();
	process_thread_->Start(this);
}

DirectXMultiPeerConductor::~DirectXMultiPeerConductor()
{
	process_thread_->Quit();
}

const map<int, scoped_refptr<DirectXPeerConductor>>& DirectXMultiPeerConductor::Peers() const
{
	return connected_peers_;
}

PeerConnectionClient& DirectXMultiPeerConductor::PeerConnection()
{
	return signalling_client_;
}

void DirectXMultiPeerConductor::ConnectSignallingAsync(const string& client_name)
{
	signalling_client_.Connect(config_->webrtc_config->server,
		config_->webrtc_config->port,
		client_name);
}

void DirectXMultiPeerConductor::SetMainWindow(MainWindow* main_window)
{
	main_window_ = main_window;
}

void DirectXMultiPeerConductor::SetDataChannelMessageHandler(const function<void(int, const string&)>& data_channel_handler)
{
	data_channel_handler_ = data_channel_handler;
}

void DirectXMultiPeerConductor::OnIceConnectionChange(int peer_id, PeerConnectionInterface::IceConnectionState new_state)
{
	// peer connected
	if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionConnected)
	{
		if (cur_capacity_ > -1)
		{
			cur_capacity_ -= cur_capacity_ >= 1 ? 1 : 0;
			signalling_client_.UpdateCapacity(cur_capacity_);
		}
	}
	// peer disconnected
	else if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		connected_peers_.erase(peer_id);

		if (cur_capacity_ > -1)
		{
			cur_capacity_ += cur_capacity_ < max_capacity_ ? 1 : 0;
			signalling_client_.UpdateCapacity(cur_capacity_);
		}
	}
}

void DirectXMultiPeerConductor::HandleDataChannelMessage(int peer_id, const string& message)
{
	if (data_channel_handler_)
	{
		data_channel_handler_(peer_id, message);
	}
}

void DirectXMultiPeerConductor::HandleSignalConnect()
{
	if (max_capacity_ > -1)
	{
		signalling_client_.UpdateCapacity(max_capacity_);
	}
}

scoped_refptr<DirectXPeerConductor> DirectXMultiPeerConductor::SafeAllocatePeerMapEntry(int peer_id)
{
	if (connected_peers_.find(peer_id) == connected_peers_.end())
	{
		string peer_name = signalling_client_.peers().at(peer_id);
		connected_peers_[peer_id] = new RefCountedObject<DirectXPeerConductor>(peer_id,
			peer_name,
			config_->webrtc_config,
			peer_factory_,
			[&, peer_id](const string& message)
		{
			message_queue_.push(MessageEntry(peer_id, message));
			rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
		},
			d3d_device_.Get());

		connected_peers_[peer_id]->SignalIceConnectionChange.connect(this, &DirectXMultiPeerConductor::OnIceConnectionChange);
		connected_peers_[peer_id]->SignalDataChannelMessage.connect(this, &DirectXMultiPeerConductor::HandleDataChannelMessage);
	}

	return connected_peers_[peer_id];
}

void DirectXMultiPeerConductor::OnSignedIn()
{
	should_process_queue_.store(true);
	if (main_window_ && main_window_->IsWindow())
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void DirectXMultiPeerConductor::OnDisconnected()
{
	should_process_queue_.store(false);
}

void DirectXMultiPeerConductor::OnPeerConnected(int id, const string& name)
{
	// Refresh the list if we're showing it.
	if (main_window_ && main_window_->IsWindow() && main_window_->current_ui() == MainWindow::LIST_PEERS)
	{
		main_window_->SwitchToPeerList(signalling_client_.peers());
	}
}

void DirectXMultiPeerConductor::OnPeerDisconnected(int peer_id)
{
	connected_peers_.erase(peer_id);
}

void DirectXMultiPeerConductor::OnMessageFromPeer(int peer_id, const string& message)
{
	auto peer = SafeAllocatePeerMapEntry(peer_id);
	peer->HandlePeerMessage(message);
}

void DirectXMultiPeerConductor::OnMessageSent(int err) {}

void DirectXMultiPeerConductor::OnServerConnectionFailure() {}

void DirectXMultiPeerConductor::OnMessage(Message* msg)
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

void DirectXMultiPeerConductor::Run(Thread* thread)
{
	while (!thread->IsQuitting())
	{
		thread->ProcessMessages(500);
	}
}

void DirectXMultiPeerConductor::StartLogin(const std::string& server, int port)
{
	signalling_client_.Connect(server, port, "renderingserver_" + GetPeerName());
}

void DirectXMultiPeerConductor::DisconnectFromServer() {}

void DirectXMultiPeerConductor::ConnectToPeer(int peer_id)
{
	auto peer = SafeAllocatePeerMapEntry(peer_id);
	if (!peer->IsConnected())
	{
		peer->AllocatePeerConnection(true);
	}
}

void DirectXMultiPeerConductor::DisconnectFromCurrentPeer() {}

void DirectXMultiPeerConductor::UIThreadCallback(int msg_id, void* data) {}

void DirectXMultiPeerConductor::Close() {}
