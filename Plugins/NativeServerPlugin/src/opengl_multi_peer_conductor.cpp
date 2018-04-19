#include "pch.h"

#include "defaults.h"
#include "opengl_multi_peer_conductor.h"

OpenGLMultiPeerConductor::OpenGLMultiPeerConductor(shared_ptr<FullServerConfig> config) :
	MultiPeerConductor(config)
{
}

scoped_refptr<PeerConductor> OpenGLMultiPeerConductor::SafeAllocatePeerMapEntry(int peer_id)
{
	if (connected_peers_.find(peer_id) == connected_peers_.end())
	{
		string peer_name = signalling_client_.peers().at(peer_id);
		connected_peers_[peer_id] = new RefCountedObject<OpenGLPeerConductor>(peer_id,
			peer_name,
			config_->webrtc_config,
			peer_factory_,
			[&, peer_id](const string& message)
		{
			message_queue_.push(MessageEntry(peer_id, message));
			rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
		});

		connected_peers_[peer_id]->SignalIceConnectionChange.connect((MultiPeerConductor*)this, &OpenGLMultiPeerConductor::OnIceConnectionChange);
		connected_peers_[peer_id]->SignalDataChannelMessage.connect((MultiPeerConductor*)this, &OpenGLMultiPeerConductor::HandleDataChannelMessage);
	}

	return connected_peers_[peer_id];
}
