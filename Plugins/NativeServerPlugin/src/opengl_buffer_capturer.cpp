#include "pch.h"

#include "opengl_buffer_capturer.h"
#include "plugindefs.h"

using namespace Microsoft::WRL;
using namespace StreamingToolkit;

#define MAX_DIMENSION		4096

OpenGLBufferCapturer::OpenGLBufferCapturer()
{
}

void OpenGLBufferCapturer::SendFrame(GLubyte* color_buffer, int width, int height)
{
	rtc::CritScope cs(&lock_);

	if (!running_ || width > MAX_DIMENSION || height > MAX_DIMENSION)
	{
		return;
	}

	rtc::scoped_refptr<webrtc::I420Buffer> buffer;
	buffer = webrtc::I420Buffer::Create(width, height);

	if (use_software_encoder_)
	{
		libyuv::ABGRToI420(
			(uint8_t*)color_buffer,
			width * 4,
			buffer.get()->MutableDataY(),
			buffer.get()->StrideY(),
			buffer.get()->MutableDataU(),
			buffer.get()->StrideU(),
			buffer.get()->MutableDataV(),
			buffer.get()->StrideV(),
			width,
			height);
	}

	auto frame = webrtc::VideoFrame(buffer, kVideoRotation_0, 0);

	if (!use_software_encoder_)
	{
		frame.set_frame_buffer((uint8_t*)color_buffer);
	}

	frame.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());

	// Sending video frame.
	BufferCapturer::SendFrame(frame);
}
