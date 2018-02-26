#pragma once

#include "pch.h"
#include "peer_conductor.h"
#include "directx_buffer_capturer.h"

// A PeerConductor using a DirectXBufferCapturer
class DirectXPeerConductor : public PeerConductor
{
public:
	DirectXPeerConductor(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtc_config,
		scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
		const function<void(const string&)>& send_func,
		ID3D11Device* d3d_device);

	void SendFrame(ID3D11Texture2D* frame_buffer, int64_t prediction_time_stamp = -1);

	void SendFrame(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer, int64_t prediction_time_stamp = -1);

protected:
	// Provide the same buffer capturer for each single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() override;

private:
	ID3D11Device* d3d_device_;
	DirectXBufferCapturer* capturer_;
};
