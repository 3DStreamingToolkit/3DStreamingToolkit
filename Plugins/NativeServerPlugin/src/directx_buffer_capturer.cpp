#include "pch.h"

#include "directx_buffer_capturer.h"
#include "plugindefs.h"

using namespace Microsoft::WRL;
using namespace StreamingToolkit;

DirectXBufferCapturer::DirectXBufferCapturer(ID3D11Device* d3d_device) :
	d3d_device_(d3d_device)
{
	// Gets the device context.
	d3d_device_->GetImmediateContext(&d3d_context_);

#ifdef MULTITHREAD_PROTECTION
	// Enables multithread protection.
	ID3D11Multithread* multithread;
	d3d_device_->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // MULTITHREAD_PROTECTION
}

void DirectXBufferCapturer::SendFrame(ID3D11Texture2D* frame_buffer, int64_t prediction_time_stamp)
{
	// The video capturer hasn't started since there is no active connection.
	if (!running_)
	{
		return;
	}

	// Updates staging frame buffer.
	UpdateStagingBuffer(frame_buffer);

	// Creates webrtc frame buffer.
	D3D11_TEXTURE2D_DESC desc;
	staging_frame_buffer_->GetDesc(&desc);
	rtc::scoped_refptr<webrtc::I420Buffer> buffer = 
		webrtc::I420Buffer::Create(desc.Width, desc.Height);

	// For software encoder, converting to supported video format.
	if (use_software_encoder_)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(d3d_context_.Get()->Map(
			staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			libyuv::ABGRToI420(
				(uint8_t*)mapped.pData,
				desc.Width * 4,
				buffer.get()->MutableDataY(),
				buffer.get()->StrideY(),
				buffer.get()->MutableDataU(),
				buffer.get()->StrideU(),
				buffer.get()->MutableDataV(),
				buffer.get()->StrideV(),
				desc.Width,
				desc.Height);

			d3d_context_->Unmap(staging_frame_buffer_.Get(), 0);
		}
	}

	// Updates time stamp.
	auto time_stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	// Creates video frame buffer.
	auto frame = webrtc::VideoFrame(buffer, kVideoRotation_0, time_stamp);
	frame.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
	frame.set_rotation(VideoRotation::kVideoRotation_0);
	frame.set_prediction_timestamp(prediction_time_stamp);

	// For hardware encoder, setting the video frame buffer.
	if (!use_software_encoder_)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(d3d_context_.Get()->Map(
			staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			frame.set_frame_buffer((uint8_t*)mapped.pData);
			d3d_context_->Unmap(staging_frame_buffer_.Get(), 0);
		}
	}

	// Sending video frame.
	BufferCapturer::SendFrame(frame);
}

void DirectXBufferCapturer::SendFrame(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer, int64_t prediction_time_stamp)
{
	// The video capturer hasn't started since there is no active connection.
	if (!running_)
	{
		return;
	}

	// Updates staging frame buffer.
	UpdateStagingBuffer(left_frame_buffer, right_frame_buffer);

	// Creates webrtc frame buffer.
	D3D11_TEXTURE2D_DESC desc;
	staging_frame_buffer_->GetDesc(&desc);
	rtc::scoped_refptr<webrtc::I420Buffer> buffer =
		webrtc::I420Buffer::Create(desc.Width, desc.Height);

	// For software encoder, converting to supported video format.
	if (use_software_encoder_)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(d3d_context_.Get()->Map(
			staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			libyuv::ABGRToI420(
				(uint8_t*)mapped.pData,
				desc.Width * 4,
				buffer.get()->MutableDataY(),
				buffer.get()->StrideY(),
				buffer.get()->MutableDataU(),
				buffer.get()->StrideU(),
				buffer.get()->MutableDataV(),
				buffer.get()->StrideV(),
				desc.Width,
				desc.Height);

			d3d_context_->Unmap(staging_frame_buffer_.Get(), 0);
		}
	}

	// Updates time stamp.
	auto time_stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	// Creates video frame buffer.
	auto frame = webrtc::VideoFrame(buffer, kVideoRotation_0, time_stamp);
	frame.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());
	frame.set_rotation(VideoRotation::kVideoRotation_0);
	frame.set_prediction_timestamp(prediction_time_stamp);

	// For hardware encoder, setting the video frame buffer.
	if (!use_software_encoder_)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(d3d_context_.Get()->Map(
			staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			frame.set_frame_buffer((uint8_t*)mapped.pData);
			d3d_context_->Unmap(staging_frame_buffer_.Get(), 0);
		}
	}

	// Sending video frame.
	BufferCapturer::SendFrame(frame);
}

void DirectXBufferCapturer::UpdateStagingBuffer(ID3D11Texture2D* frame_buffer)
{
	D3D11_TEXTURE2D_DESC desc;
	frame_buffer->GetDesc(&desc);

	// Lazily initializes the staging frame buffer.
	if (!staging_frame_buffer_)
	{
		staging_frame_buffer_desc_ = { 0 };
		staging_frame_buffer_desc_.ArraySize = 1;
		staging_frame_buffer_desc_.Format = desc.Format;
		staging_frame_buffer_desc_.Width = desc.Width;
		staging_frame_buffer_desc_.Height = desc.Height;
		staging_frame_buffer_desc_.MipLevels = 1;
		staging_frame_buffer_desc_.SampleDesc.Count = 1;
		staging_frame_buffer_desc_.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		staging_frame_buffer_desc_.Usage = D3D11_USAGE_STAGING;
		d3d_device_->CreateTexture2D(
			&staging_frame_buffer_desc_, nullptr, &staging_frame_buffer_);
	}
	// Resizes if needed.
	else if (staging_frame_buffer_desc_.Width != desc.Width || 
		staging_frame_buffer_desc_.Height != desc.Height)
	{
		staging_frame_buffer_desc_.Width = desc.Width;
		staging_frame_buffer_desc_.Height = desc.Height;
		d3d_device_->CreateTexture2D(&staging_frame_buffer_desc_, nullptr,
			&staging_frame_buffer_);
	}

	// Copies the frame buffer to the staging one.		
	d3d_context_->CopyResource(staging_frame_buffer_.Get(), frame_buffer);
}

void DirectXBufferCapturer::UpdateStagingBuffer(ID3D11Texture2D* left_frame_buffer, ID3D11Texture2D* right_frame_buffer)
{
	D3D11_TEXTURE2D_DESC desc;
	left_frame_buffer->GetDesc(&desc);

	// Lazily initializes the staging frame buffer.
	if (!staging_frame_buffer_)
	{
		staging_frame_buffer_desc_ = { 0 };
		staging_frame_buffer_desc_.ArraySize = 1;
		staging_frame_buffer_desc_.Format = desc.Format;
		staging_frame_buffer_desc_.Width = desc.Width * 2;
		staging_frame_buffer_desc_.Height = desc.Height;
		staging_frame_buffer_desc_.MipLevels = 1;
		staging_frame_buffer_desc_.SampleDesc.Count = 1;
		staging_frame_buffer_desc_.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		staging_frame_buffer_desc_.Usage = D3D11_USAGE_STAGING;
		d3d_device_->CreateTexture2D(
			&staging_frame_buffer_desc_, nullptr, &staging_frame_buffer_);
	}
	// Resizes if needed.
	else if ((staging_frame_buffer_desc_.Width / 2) != desc.Width ||
		staging_frame_buffer_desc_.Height != desc.Height)
	{
		staging_frame_buffer_desc_.Width = desc.Width * 2;
		staging_frame_buffer_desc_.Height = desc.Height;
		d3d_device_->CreateTexture2D(&staging_frame_buffer_desc_, nullptr,
			&staging_frame_buffer_);
	}

	// Copies the left and right frame buffers to the staging one.		
	d3d_context_->CopySubresourceRegion(staging_frame_buffer_.Get(), 0, 0, 0, 0, 
		left_frame_buffer, 0, 0);

	d3d_context_->CopySubresourceRegion(staging_frame_buffer_.Get(), 0, desc.Width, 0, 0,
		right_frame_buffer, 0, 0);
}
