#pragma once

#include "pch.h"
#include "video_helper.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvFileIO.h"
#include "webrtc/modules/video_coding/codecs/h264/include/nvUtils.h"

using namespace Toolkit3DLibrary;

// Constructor for VideoHelper.
VideoHelper::VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context)
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

// Initializes the swap chain and IO buffers.
NVENCSTATUS VideoHelper::Initialize(IDXGISwapChain* swapChain)
{
	// Caches the pointer to the swap chain.
	m_swapChain = swapChain;
	m_frameBuffer = NULL;

	// Gets the swap chain desc.
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);	
	
	// Initializes the staging frame buffer, backed by ID3D11Texture2D*.
	m_stagingFrameBufferDesc = { 0 };
	m_stagingFrameBufferDesc.ArraySize = 1;
	m_stagingFrameBufferDesc.Format = swapChainDesc.BufferDesc.Format;
	m_stagingFrameBufferDesc.Width = swapChainDesc.BufferDesc.Width;
	m_stagingFrameBufferDesc.Height = swapChainDesc.BufferDesc.Height;
	m_stagingFrameBufferDesc.MipLevels = 1;
	m_stagingFrameBufferDesc.SampleDesc.Count = 1;
	m_stagingFrameBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	m_stagingFrameBufferDesc.Usage = D3D11_USAGE_STAGING;
	m_d3dDevice->CreateTexture2D(&m_stagingFrameBufferDesc, nullptr, &m_stagingFrameBuffer);
	
	return NV_ENC_SUCCESS;
}


// Initializes the IO buffers without access to a swapchain
NVENCSTATUS VideoHelper::Initialize(ID3D11Texture2D* frameBuffer, DXGI_FORMAT format, int width, int height)
{
	// Caches the pointer to the swap chain.
	m_swapChain = NULL;
	m_frameBuffer = frameBuffer;
	m_frameBuffer->AddRef();

	// Initializes the staging frame buffer, backed by ID3D11Texture2D*.
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

	return NV_ENC_SUCCESS;
}



// Cleanup resources.
NVENCSTATUS VideoHelper::Deinitialize()
{
	return NV_ENC_SUCCESS;
}

void VideoHelper::GetWidthAndHeight(int* width, int* height)
{
	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;
}

// Captures frame buffer from the swap chain and returns texture.
ID3D11Texture2D* VideoHelper::Capture2DTexture(int* width, int* height)
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

	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;

	return frameBuffer;
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
		// Copies the frame buffer to the staging frame buffer.
		m_d3dContext->CopyResource(m_stagingFrameBuffer, frameBuffer);
		frameBuffer->Release();

		// Accesses the frame buffer.
		D3D11_TEXTURE2D_DESC desc;
		m_stagingFrameBuffer->GetDesc(&desc);
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(m_d3dContext->Map(m_stagingFrameBuffer, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			*buffer = mapped.pData;
			*size = mapped.RowPitch * desc.Height;
			*width = desc.Width;
			*height = desc.Height;
		}

		m_d3dContext->Unmap(m_stagingFrameBuffer, 0);
	}
}
