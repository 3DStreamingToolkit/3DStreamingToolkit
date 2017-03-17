#pragma once

#include "pch.h"

#include "VideoHelper.h"

using namespace Toolkit3DLibrary;

// Constructor for VideoHelper.
VideoHelper::VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context)
{
}

// Destructor for VideoHelper.
VideoHelper::~VideoHelper()
{
}

// Caches pointer to the swap chain.
void VideoHelper::Initialize(IDXGISwapChain* swapChain)
{
	m_swapChain = swapChain;
}

// Starts capturing the video stream.
void VideoHelper::StartCapture()
{
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture()
{
	ID3D11DeviceContext* context = m_d3dContext;
	IDXGISwapChain* swapChain = m_swapChain;
	ID3D11Texture2D* frameBuffer;
	HRESULT hr = m_swapChain->GetBuffer(0, 
		__uuidof(ID3D11Texture2D), 
		reinterpret_cast< void** >(&frameBuffer));

	if (SUCCEEDED(hr))
	{
		// Captures video stream.			
	}
}

// Saves video output file.
void VideoHelper::EndCapture()
{
}
