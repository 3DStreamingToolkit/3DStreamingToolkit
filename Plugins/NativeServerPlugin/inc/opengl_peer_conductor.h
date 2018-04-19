#pragma once

#include "pch.h"
#include "opengl_buffer_capturer.h"
#include "peer_conductor.h"

// A PeerConductor using a OpenGLBufferCapturer
class OpenGLPeerConductor : public PeerConductor
{
public:
	OpenGLPeerConductor(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtc_config,
		scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
		const function<void(const string&)>& send_func);

	void SendFrame(GLubyte* color_buffer, int width, int height);

protected:
	// Provide the same buffer capturer for each single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() override;

private:
	OpenGLBufferCapturer* capturer_;
};
