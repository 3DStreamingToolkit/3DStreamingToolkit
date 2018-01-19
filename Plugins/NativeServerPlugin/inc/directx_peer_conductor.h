#pragma once

#include "pch.h"
#include "peer_conductor.h"

// A PeerConductor using a DirectXBufferCapturer
class DirectXPeerConductor : public PeerConductor,
	public has_slots<>
{
public:
	DirectXPeerConductor(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtcConfig,
		scoped_refptr<PeerConnectionFactoryInterface> peerFactory,
		const function<void(const string&)>& sendFunc,
		ID3D11Device* d3dDevice,
		bool enableSoftware = false);

	void SendFrame(ID3D11Texture2D* frame_buffer);

protected:
	// Provide the same buffer capturer for each single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() override;

private:
	ID3D11Device * m_d3dDevice;
	bool m_enableSoftware;
	vector<DirectXBufferCapturer*> m_spies;

	void OnAllocatedPtrDeleted(BufferCapturer* ptr);
};