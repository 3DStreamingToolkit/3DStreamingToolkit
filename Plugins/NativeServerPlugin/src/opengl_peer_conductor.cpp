#include "pch.h"
#include "opengl_peer_conductor.h"

OpenGLPeerConductor::OpenGLPeerConductor(int id,
	const string& name,
	shared_ptr<WebRTCConfig> webrtc_config,
	scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
	const function<void(const string&)>& send_func) : PeerConductor(
		id,
		name,
		webrtc_config,
		peer_factory,
		send_func
	)
{
}

void OpenGLPeerConductor::SendFrame(GLubyte* color_buffer, int width, int height)
{
	if (capturer_)
	{
		capturer_->SendFrame(color_buffer, width, height);
	}
}

unique_ptr<cricket::VideoCapturer> OpenGLPeerConductor::AllocateVideoCapturer()
{
	unique_ptr<OpenGLBufferCapturer> owned_ptr(new OpenGLBufferCapturer());
	capturer_ = owned_ptr.get();
	return owned_ptr;
}
