#include "pch.h"

#include "defaults.h"
#include "directx_multi_peer_conductor.h"

DirectXMultiPeerConductor::DirectXMultiPeerConductor(shared_ptr<WebRTCConfig> config,
	ID3D11Device* d3d_device) :
	webrtc_config_(config),
	d3d_device_(d3d_device),
	main_window_(nullptr)
{
	signalling_client_.RegisterObserver(this);
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
	signalling_client_.Connect(webrtc_config_->server, webrtc_config_->port, client_name);
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
	// peer disconnected
	if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		connected_peers_.erase(peer_id);
	}
}

void DirectXMultiPeerConductor::HandleDataChannelMessage(int peer_id, const string& message)
{
	if (data_channel_handler_)
	{
		data_channel_handler_(peer_id, message);
	}
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

	connected_peers_[id]->SignalIceConnectionChange.connect(this, &DirectXMultiPeerConductor::OnIceConnectionChange);
	connected_peers_[id]->SignalDataChannelMessage.connect(this, &DirectXMultiPeerConductor::HandleDataChannelMessage);

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
	connected_peers_[peer_id]->HandlePeerMessage(message);
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

void DirectXMultiPeerConductor::DisconnectFromCurrentPeer() {}

void DirectXMultiPeerConductor::UIThreadCallback(int msg_id, void* data) {}

void DirectXMultiPeerConductor::Close() {}
