#include "pch.h"
#include "directx_peer_conductor.h"

DirectXPeerConductor::DirectXPeerConductor(int id,
	const string& name,
	shared_ptr<WebRTCConfig> webrtc_config,
	scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
	const function<void(const string&)>& send_func,
	ID3D11Device* d3d_device,
	bool enable_software_encoder) : PeerConductor(
		id,
		name,
		webrtc_config,
		peer_factory,
		send_func
	),
	d3d_device_(d3d_device),
	enable_software_encoder_(enable_software_encoder) {}

void DirectXPeerConductor::SendFrame(ID3D11Texture2D* frame_buffer)
{
	if (capturer_)
	{
		capturer_->SendFrame(frame_buffer);
	}
}

unique_ptr<cricket::VideoCapturer> DirectXPeerConductor::AllocateVideoCapturer()
{
	unique_ptr<DirectXBufferCapturer> owned_ptr(new DirectXBufferCapturer(d3d_device_));

	owned_ptr->Initialize();

	if (enable_software_encoder_)
	{
		owned_ptr->EnableSoftwareEncoder();
	}

	capturer_ = owned_ptr.get();
	return owned_ptr;
}
