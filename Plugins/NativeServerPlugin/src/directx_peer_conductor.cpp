#include "pch.h"
#include "directx_peer_conductor.h"

DirectXPeerConductor::DirectXPeerConductor(int id,
	const string& name,
	shared_ptr<WebRTCConfig> webrtc_config,
	scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
	const function<void(const string&)>& send_func,
	ID3D11Device* d3d_device) : PeerConductor(
		id,
		name,
		webrtc_config,
		peer_factory,
		send_func
	),
	d3d_device_(d3d_device)
{
}

void DirectXPeerConductor::SendFrame(ID3D11Texture2D* frame_buffer, int64_t prediction_time_stamp)
{
	if (capturer_)
	{
		capturer_->SendFrame(frame_buffer, prediction_time_stamp);
	}
}

void DirectXPeerConductor::SendFrame(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer, int64_t prediction_time_stamp)
{
	if (capturer_)
	{
		capturer_->SendFrame(left_frame_buffer, right_frame_buffer, prediction_time_stamp);
	}
}

unique_ptr<cricket::VideoCapturer> DirectXPeerConductor::AllocateVideoCapturer()
{
	unique_ptr<DirectXBufferCapturer> owned_ptr(new DirectXBufferCapturer(d3d_device_));
	capturer_ = owned_ptr.get();
	return owned_ptr;
}
