#pragma once

#include "pch.h"
#include "video_helper.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvFileIO.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvUtils.h"

using namespace StreamingToolkit;

// Constructor for VideoHelper.
VideoHelper::VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context),
	m_swapChain(nullptr),
	m_frameBuffer(nullptr)
{
#ifdef MULTITHREAD_PROTECTION
	// Enables multithread protection.
	ID3D11Multithread* multithread;
	m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // MULTITHREAD_PROTECTION

	webrtc::H264EncoderImpl::SetDevice(device);
	webrtc::H264EncoderImpl::SetContext(context);
}

// Destructor for VideoHelper.
VideoHelper::~VideoHelper()
{
	SAFE_RELEASE(m_stagingFrameBuffer);
}

// Initializes the staging frame buffer.
void VideoHelper::Initialize(DXGI_FORMAT format, int width, int height)
{
	m_stagingFrameBufferDesc = { 0 };
	m_stagingFrameBufferDesc.ArraySize = 1;
	m_stagingFrameBufferDesc.Format = format;
	m_stagingFrameBufferDesc.Width = width;
	m_stagingFrameBufferDesc.Height = height;
	m_stagingFrameBufferDesc.MipLevels = 1;
	m_stagingFrameBufferDesc.SampleDesc.Count = 1;
	m_stagingFrameBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	m_stagingFrameBufferDesc.Usage = D3D11_USAGE_STAGING;
	m_d3dDevice->CreateTexture2D(&m_stagingFrameBufferDesc, nullptr, &m_stagingFrameBuffer);
}

void VideoHelper::Initialize(IDXGISwapChain* swapChain)
{
	// Caches the pointer to the swap chain.
	m_swapChain = swapChain;

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);	
	
	Initialize(swapChainDesc.BufferDesc.Format,
		swapChainDesc.BufferDesc.Width, swapChainDesc.BufferDesc.Height);
}

// Initializes the IO buffers without access to a swapchain
void VideoHelper::Initialize(ID3D11Texture2D* frameBuffer, DXGI_FORMAT format, int width, int height)
{
	// Caches the pointer to the swap chain's buffer.
	m_frameBuffer = frameBuffer;
	m_frameBuffer->AddRef();

	Initialize(format, width, height);
}

void VideoHelper::UpdateStagingBuffer(ID3D11Texture2D* frameBuffer)
{
	// Resizes if needed.
	D3D11_TEXTURE2D_DESC textureDesc;
	frameBuffer->GetDesc(&textureDesc);
	if (m_stagingFrameBufferDesc.Width != textureDesc.Width ||
		m_stagingFrameBufferDesc.Height != textureDesc.Height)
	{
		m_stagingFrameBufferDesc.Width = textureDesc.Width;
		m_stagingFrameBufferDesc.Height = textureDesc.Height;
		SAFE_RELEASE(m_stagingFrameBuffer);
		m_d3dDevice->CreateTexture2D(&m_stagingFrameBufferDesc, nullptr, 
			&m_stagingFrameBuffer);
	}

	// Copies the frame buffer to the staging frame buffer.
	m_d3dContext->CopyResource(m_stagingFrameBuffer, frameBuffer);
	frameBuffer->Release();
}

void VideoHelper::GetWidthAndHeight(int* width, int* height)
{
	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture(ID3D11Texture2D** texture, int* width, int* height)
{
	// Gets the frame buffer from the swap chain.
	ID3D11Texture2D* frameBuffer = nullptr;

	if (m_swapChain)
	{
		// Gets the frame buffer from the swap chain.
		HRESULT hr = m_swapChain->GetBuffer(0,
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&frameBuffer));
	}
	else
	{
		frameBuffer = m_frameBuffer;
		m_frameBuffer->AddRef();
	}

	if (frameBuffer)
	{
		// Updates the staging frame buffer and returns.
		UpdateStagingBuffer(frameBuffer);
		*texture = m_stagingFrameBuffer;
		m_stagingFrameBuffer->AddRef();
		*width = m_stagingFrameBufferDesc.Width;
		*height = m_stagingFrameBufferDesc.Height;
	}
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture(void** buffer, int* size, int* width, int* height)
{
	ID3D11Texture2D* frameBuffer = nullptr;

	if (m_swapChain)
	{
		// Gets the frame buffer from the swap chain.
		HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&frameBuffer));
	}
	else
	{
		frameBuffer = m_frameBuffer;
		frameBuffer->AddRef();
	}

	if (frameBuffer)
	{
		// Updates the staging frame buffer and returns.
		UpdateStagingBuffer(frameBuffer);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_d3dContext->Map(m_stagingFrameBuffer, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			*buffer = mapped.pData;
			*size = mapped.RowPitch * m_stagingFrameBufferDesc.Height;
			*width = m_stagingFrameBufferDesc.Width;
			*height = m_stagingFrameBufferDesc.Height;
		}

		m_d3dContext->Unmap(m_stagingFrameBuffer, 0);
	}
}
