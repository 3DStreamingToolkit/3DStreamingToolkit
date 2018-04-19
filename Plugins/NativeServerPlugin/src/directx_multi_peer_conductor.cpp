#include "pch.h"

#include "directx_multi_peer_conductor.h"

DirectXMultiPeerConductor::DirectXMultiPeerConductor(shared_ptr<FullServerConfig> config,
	ID3D11Device* d3d_device) : 
	MultiPeerConductor(config),
	d3d_device_(d3d_device)
{
}

scoped_refptr<PeerConductor> DirectXMultiPeerConductor::SafeAllocatePeerMapEntry(int peer_id)
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

		connected_peers_[peer_id]->SignalIceConnectionChange.connect((MultiPeerConductor*)this, &MultiPeerConductor::OnIceConnectionChange);
		connected_peers_[peer_id]->SignalDataChannelMessage.connect((MultiPeerConductor*)this, &MultiPeerConductor::HandleDataChannelMessage);
	}

	return connected_peers_[peer_id];
}
