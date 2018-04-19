#pragma once

#include "pch.h"

#include "opengl_peer_conductor.h"
#include "multi_peer_conductor.h"

class OpenGLMultiPeerConductor : public MultiPeerConductor
{
public:
	OpenGLMultiPeerConductor(shared_ptr<FullServerConfig> config);

	scoped_refptr<PeerConductor> SafeAllocatePeerMapEntry(int peer_id) override;
};
