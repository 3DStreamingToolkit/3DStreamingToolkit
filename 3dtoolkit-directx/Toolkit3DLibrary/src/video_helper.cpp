#pragma once

#include "pch.h"
#include "video_helper.h"
#include "nvFileIO.h"
#include "nvUtils.h"

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

#ifdef USE_WEBRTC_NVENCODE
	webrtc::H264EncoderImpl::SetDevice(device);
	webrtc::H264EncoderImpl::SetContext(context);
#endif // USE_WEBRTC_NVENCODE
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
	HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer));

	*width = m_stagingFrameBufferDesc.Width;
	*height = m_stagingFrameBufferDesc.Height;

	return frameBuffer;
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture(void** buffer, int* size, int* width, int* height)
{
	// Gets the frame buffer from the swap chain.
	ID3D11Texture2D* frameBuffer = nullptr;
	CHECK_HR_FAILED(m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer)));

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
