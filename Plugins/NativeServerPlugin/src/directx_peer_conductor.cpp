#include "pch.h"
#include "directx_peer_conductor.h"

DirectXPeerConductor::DirectXPeerConductor(int id,
	const string& name,
	shared_ptr<WebRTCConfig> webrtcConfig,
	scoped_refptr<PeerConnectionFactoryInterface> peerFactory,
	const function<void(const string&)>& sendFunc,
	ID3D11Device* d3dDevice,
	bool enableSoftware) : PeerConductor(
		id,
		name,
		webrtcConfig,
		peerFactory,
		sendFunc
	),
	m_d3dDevice(d3dDevice),
	m_enableSoftware(enableSoftware) {}

void DirectXPeerConductor::SendFrame(ID3D11Texture2D* frame_buffer)
{
	for each (auto spy in m_spies)
	{
		spy->SendFrame(frame_buffer);
	}
}

unique_ptr<cricket::VideoCapturer> DirectXPeerConductor::AllocateVideoCapturer()
{
	unique_ptr<DirectXBufferCapturer> ownedPtr(new DirectXBufferCapturer(m_d3dDevice));

	ownedPtr->Initialize();

	if (m_enableSoftware)
	{
		ownedPtr->EnableSoftwareEncoder();
	}

	// rely on the deleter signal to let us know when it's gone and we should stop updating it
	ownedPtr->SignalDestroyed.connect(this, &DirectXPeerConductor::OnAllocatedPtrDeleted);

	// spy on this pointer. this only works because webrtc doesn't std::move it
	m_spies.push_back(ownedPtr.get());

	return ownedPtr;
}

void DirectXPeerConductor::OnAllocatedPtrDeleted(BufferCapturer* ptr)
{
	// see https://stackoverflow.com/questions/347441/erasing-elements-from-a-vector
	m_spies.erase(std::remove(m_spies.begin(), m_spies.end(), ptr), m_spies.end());
}