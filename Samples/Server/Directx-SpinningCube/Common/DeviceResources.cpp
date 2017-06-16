#include "pch.h"
#include "DeviceResources.h"
#include "macros.h"

using namespace DX;

// Constructor for DeviceResources.
DeviceResources::DeviceResources() :
#ifdef NO_UI
	m_frameBuffer(nullptr)
#else
	m_swapChain(nullptr)
#endif
{
	CreateDeviceResources();
}

// Destructor for DeviceResources.
DeviceResources::~DeviceResources()
{
	CleanupResources();
}

void DeviceResources::CleanupResources()
{
	delete []m_screenViewport;
	SAFE_RELEASE(m_d3dContext);
	SAFE_RELEASE(m_d3dRenderTargetView);
	SAFE_RELEASE(m_d3dDevice);

#ifdef NO_UI
	SAFE_RELEASE(m_frameBuffer);
#else
	SAFE_RELEASE(m_swapChain);
#endif
}

SIZE DeviceResources::GetOutputSize() const
{
	return m_outputSize;
}

ID3D11Device1* DeviceResources::GetD3DDevice() const
{
	return m_d3dDevice;
}

ID3D11DeviceContext1* DeviceResources::GetD3DDeviceContext() const
{
	return m_d3dContext;
}

#ifdef NO_UI
ID3D11Texture2D* DeviceResources::GetFrameBuffer() const
{
	return m_frameBuffer;
}
#else
IDXGISwapChain1* DeviceResources::GetSwapChain() const
{
	return m_swapChain;
}
#endif

ID3D11RenderTargetView* DeviceResources::GetBackBufferRenderTargetView() const
{
	return m_d3dRenderTargetView;
}

D3D11_VIEWPORT* DeviceResources::GetScreenViewport() const
{
	return m_screenViewport;
}

// Configures the Direct3D device, and stores handles to it and the device context.
HRESULT DeviceResources::CreateDeviceResources()
{
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	HRESULT hr = S_OK;
	UINT createDeviceFlags = 0;

	// Creates D3D11 device.
	hr = D3D11CreateDevice(
		nullptr, 
		D3D_DRIVER_TYPE_HARDWARE, 
		nullptr, 
		createDeviceFlags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr, 
		&context);

	if (FAILED(hr))
	{
		return hr;
	}

	hr = device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&m_d3dDevice));
	if (SUCCEEDED(hr))
	{
		(void)context->QueryInterface(
			__uuidof(ID3D11DeviceContext1), 
			reinterpret_cast<void**>(&m_d3dContext));
	}

	// Cleanup.
	SAFE_RELEASE(context);
	SAFE_RELEASE(device);

	return hr;
}

// These resources need to be recreated every time the window size is changed.
HRESULT DeviceResources::CreateWindowSizeDependentResources(HWND hWnd)
{
	// Obtains DXGI factory from device.
	HRESULT hr = S_OK;
	ID3D11Texture2D* frameBuffer = nullptr;

#ifdef NO_UI
	D3D11_TEXTURE2D_DESC description;

	memset(&description, 0, sizeof(D3D11_TEXTURE2D_DESC));

	description.Height = FRAME_BUFFER_HEIGHT;
	description.Width = FRAME_BUFFER_WIDTH;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	description.Usage = D3D11_USAGE_DEFAULT;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.SampleDesc.Count = 1;
	description.SampleDesc.Quality = 0;
	description.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	hr = m_d3dDevice->CreateTexture2D(&description, nullptr, &m_frameBuffer);
	if (FAILED(hr))
	{
		return hr;
	}

	frameBuffer = m_frameBuffer;
#else
	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIFactory1* dxgiFactory = nullptr;
	hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
	if (SUCCEEDED(hr))
	{
		IDXGIAdapter* adapter = nullptr;
		hr = dxgiDevice->GetAdapter(&adapter);
		if (SUCCEEDED(hr))
		{
			hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
			adapter->Release();
		}

		dxgiDevice->Release();
	}

	if (FAILED(hr))
	{
		return hr;
	}

	// Creates swap chain.
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2; // Front and back buffer to swap
		swapChainDesc.SampleDesc.Count = 1; // Disable anti-aliasing
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapChainDesc.Width = FRAME_BUFFER_WIDTH;
		swapChainDesc.Height = FRAME_BUFFER_HEIGHT;

		hr = dxgiFactory2->CreateSwapChainForHwnd(
			m_d3dDevice, 
			hWnd, 
			&swapChainDesc,
			nullptr, 
			nullptr, 
			&m_swapChain);

		if (FAILED(hr))
		{
			return hr;
		}

		dxgiFactory2->Release();
	}

	// Cleanup.
	SAFE_RELEASE(dxgiFactory);

	// Creates the render target view.
	
	hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frameBuffer));
	if (FAILED(hr))
	{
		return hr;
	}
#endif

	hr = m_d3dDevice->CreateRenderTargetView(frameBuffer, nullptr, &m_d3dRenderTargetView);
	SAFE_RELEASE(frameBuffer);
	if (FAILED(hr))
	{
		return hr;
	}

	// Initializes the viewport.
#ifdef STEREO_OUTPUT_MODE
	m_screenViewport = new D3D11_VIEWPORT[2];

	// Left eye.
	m_screenViewport[0] = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		(FLOAT)FRAME_BUFFER_WIDTH / 2,
		(FLOAT)FRAME_BUFFER_HEIGHT);

	// Right eye.
	m_screenViewport[1] = CD3D11_VIEWPORT(
		(FLOAT)FRAME_BUFFER_WIDTH / 2,
		0.0f,
		(FLOAT)FRAME_BUFFER_WIDTH / 2,
		(FLOAT)FRAME_BUFFER_HEIGHT);
#else // STEREO_OUTPUT_MODE
	m_screenViewport = new D3D11_VIEWPORT[1];
	m_screenViewport[0] = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		(FLOAT)FRAME_BUFFER_WIDTH,
		(FLOAT)FRAME_BUFFER_HEIGHT);

	m_d3dContext->RSSetViewports(1, m_screenViewport);
#endif // STEREO_OUTPUT_MODE

	m_outputSize.cx = FRAME_BUFFER_WIDTH;
	m_outputSize.cy = FRAME_BUFFER_HEIGHT;

	return hr;
}

void DeviceResources::SetWindow(HWND hWnd)
{
	CreateWindowSizeDependentResources(hWnd);
}

// Presents the contents of the swap chain to the screen.
void DeviceResources::Present()
{
#ifndef NO_UI
	if (m_swapChain != nullptr)
	{
		m_swapChain->Present(1, 0);
	}
#endif
}
