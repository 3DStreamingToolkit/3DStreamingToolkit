#pragma once

#include "pch.h"

#include "directx_peer_conductor.h"
#include "multi_peer_conductor.h"

class DirectXMultiPeerConductor : public MultiPeerConductor
{
public:
	DirectXMultiPeerConductor(shared_ptr<FullServerConfig> config, ID3D11Device* d3d_device);

private:
	// Handles creation of a new peer entry in connected_peers_ if needed
	scoped_refptr<PeerConductor> SafeAllocatePeerMapEntry(int peer_id) override;

	ComPtr<ID3D11Device> d3d_device_;
};
