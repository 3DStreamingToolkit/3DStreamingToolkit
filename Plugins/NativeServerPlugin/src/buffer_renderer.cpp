#pragma once

#include "pch.h"
#include "buffer_renderer.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvFileIO.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvUtils.h"

using namespace StreamingToolkit;
using namespace Microsoft::WRL;

// Constructor for BufferRenderer.
BufferRenderer::BufferRenderer(
	int width,
	int height,
	ID3D11Device* device,
	const std::function<void()>& frame_render_func,
	ID3D11Texture2D* frame_buffer) :
		width_(width),
		height_(height),
		d3d_device_(device),
		frame_render_func_(frame_render_func),
		frame_buffer_(frame_buffer),
		sending_lock_(false)
{
	// Gets the device context.
	d3d_device_->GetImmediateContext(&d3d_context_);

	// System service (headless mode)
	D3D11_TEXTURE2D_DESC frame_buffer_desc = { 0 };
	if (frame_buffer == nullptr)
	{
		// Initializes the frame buffer description.
		frame_buffer_desc.ArraySize = 1;
		frame_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		frame_buffer_desc.Width = width;
		frame_buffer_desc.Height = height;
		frame_buffer_desc.MipLevels = 1;
		frame_buffer_desc.SampleDesc.Count = 1;
		frame_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
		frame_buffer_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

		// Creates the frame buffer.
		d3d_device_->CreateTexture2D(&frame_buffer_desc, nullptr, &frame_buffer_);

		// Creates the render target view.
		d3d_device_->CreateRenderTargetView(frame_buffer_.Get(), nullptr, &d3d_render_target_view_);
	}
	else
	{
		frame_buffer_->GetDesc(&frame_buffer_desc);
	}

#ifdef MULTITHREAD_PROTECTION
	// Enables multithread protection.
	ID3D11Multithread* multithread;
	d3d_device_->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // MULTITHREAD_PROTECTION

	webrtc::H264EncoderImpl::SetDevice(d3d_device_.Get());
	webrtc::H264EncoderImpl::SetContext(d3d_context_.Get());

	// Initializes the staging frame buffer
	staging_frame_buffer_desc_ = { 0 };
	staging_frame_buffer_desc_.ArraySize = 1;
	staging_frame_buffer_desc_.Format = frame_buffer_desc.Format;
	staging_frame_buffer_desc_.Width = width;
	staging_frame_buffer_desc_.Height = height;
	staging_frame_buffer_desc_.MipLevels = 1;
	staging_frame_buffer_desc_.SampleDesc.Count = 1;
	staging_frame_buffer_desc_.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	staging_frame_buffer_desc_.Usage = D3D11_USAGE_STAGING;
	d3d_device_->CreateTexture2D(&staging_frame_buffer_desc_, nullptr, &staging_frame_buffer_);
}

// Destructor for BufferRenderer.
BufferRenderer::~BufferRenderer()
{
	d3d_device_.Reset();
	d3d_context_.Reset();
	frame_buffer_.Reset();
	staging_frame_buffer_.Reset();
	d3d_render_target_view_.Reset();
}

void BufferRenderer::GetDimension(int* width, int* height)
{
	*width = width_;
	*height = height_;
}

ID3D11RenderTargetView* BufferRenderer::GetRenderTargetView()
{
	auto lock = buffer_lock_.Lock();
	return d3d_render_target_view_.Get();
}

void BufferRenderer::Render()
{
	auto lock = buffer_lock_.Lock();
	if (frame_buffer_)
	{
		frame_render_func_();
	}
}

ID3D11Texture2D* BufferRenderer::Capture()
{
	auto lock = buffer_lock_.Lock();

	if (!frame_buffer_)
	{
		return nullptr;
	}

	// Updates frame buffer size.
	D3D11_TEXTURE2D_DESC desc;
	frame_buffer_->GetDesc(&desc);
	width_ = desc.Width;
	height_ = desc.Height;

	return frame_buffer_.Get();
}

void BufferRenderer::Capture(void** buffer, int* size)
{
	auto lock = buffer_lock_.Lock();

	if (!frame_buffer_ || !staging_frame_buffer_)
	{
		return;
	}

	// Updates the staging buffer before capturing.
	UpdateStagingBuffer();

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(d3d_context_.Get()->Map(staging_frame_buffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
	{
		*buffer = mapped.pData;
		*size = mapped.RowPitch * staging_frame_buffer_desc_.Height;
	}

	d3d_context_->Unmap(staging_frame_buffer_.Get(), 0);
}

void BufferRenderer::UpdateStagingBuffer()
{
	// Resizes if needed.		
	D3D11_TEXTURE2D_DESC desc;
	frame_buffer_->GetDesc(&desc);
	if (staging_frame_buffer_desc_.Width != desc.Width ||
		staging_frame_buffer_desc_.Height != desc.Height)
	{
		width_ = desc.Width;
		height_ = desc.Height;
		staging_frame_buffer_desc_.Width = desc.Width;
		staging_frame_buffer_desc_.Height = desc.Height;
		d3d_device_->CreateTexture2D(&staging_frame_buffer_desc_, nullptr,
			&staging_frame_buffer_);
	}
	
	// Copies the frame buffer to the staging frame buffer.		
	d3d_context_->CopyResource(staging_frame_buffer_.Get(), frame_buffer_.Get());
}

void BufferRenderer::Release()
{
	auto lock = buffer_lock_.Lock();
	frame_buffer_.Reset();
}

void BufferRenderer::Resize(ID3D11Texture2D* frame_buffer)
{
	auto lock = buffer_lock_.Lock();
	frame_buffer_ = frame_buffer;
}

void BufferRenderer::Resize(int width, int height)
{
	auto lock = buffer_lock_.Lock();

	// Initializes the frame buffer description.
	D3D11_TEXTURE2D_DESC frame_buffer_desc = { 0 };
	frame_buffer_desc.ArraySize = 1;
	frame_buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	frame_buffer_desc.Width = width;
	frame_buffer_desc.Height = height;
	frame_buffer_desc.MipLevels = 1;
	frame_buffer_desc.SampleDesc.Count = 1;
	frame_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	frame_buffer_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	// Creates the frame buffer.
	d3d_device_->CreateTexture2D(&frame_buffer_desc, nullptr, &frame_buffer_);

	// Creates the render target view.
	d3d_device_->CreateRenderTargetView(frame_buffer_.Get(), nullptr, &d3d_render_target_view_);
}

void BufferRenderer::SetPredictionTimestamp(int64_t prediction_timestamp)
{
	auto lock = buffer_lock_.Lock();
	prediction_timestamp_ = prediction_timestamp;
}

int64_t BufferRenderer::GetPredictionTimestamp()
{
	auto lock = buffer_lock_.Lock();
	return prediction_timestamp_;
}

void BufferRenderer::Lock()
{
	auto lock = buffer_lock_.Lock();
	sending_lock_ = true;
}

void BufferRenderer::Unlock()
{
	auto lock = buffer_lock_.Lock();
	sending_lock_ = false;
}

bool BufferRenderer::IsLocked()
{
	auto lock = buffer_lock_.Lock();
	return sending_lock_;
}