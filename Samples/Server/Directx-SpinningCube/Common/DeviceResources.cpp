#include "pch.h"
#include "DeviceResources.h"
#include "macros.h"

using namespace DX;

// Constructor for DeviceResources.
DeviceResources::DeviceResources(int width, int height) :
	m_outputSize({ width, height }),
	m_isStereo(false),
	m_d3dDevice(nullptr),
	m_d3dContext(nullptr),
	m_d3dRenderTargetView(nullptr),
	m_swapChain(nullptr),
	m_screenViewport(nullptr)
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
	if (m_screenViewport)
	{
		delete []m_screenViewport;
	}

	SAFE_RELEASE(m_d3dContext);
	SAFE_RELEASE(m_d3dRenderTargetView);
	SAFE_RELEASE(m_swapChain);
	SAFE_RELEASE(m_d3dDevice);
}

SIZE DeviceResources::GetOutputSize() const
{
	return m_outputSize;
}

bool DeviceResources::IsStereo() const
{
	return m_isStereo;
}

ID3D11Device1* DeviceResources::GetD3DDevice() const
{
	return m_d3dDevice;
}

ID3D11DeviceContext1* DeviceResources::GetD3DDeviceContext() const
{
	return m_d3dContext;
}

IDXGISwapChain1* DeviceResources::GetSwapChain() const
{
	return m_swapChain;
}

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
	HRESULT hr = S_OK;

	if (hWnd)
	{
		// Obtains DXGI factory from device.
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
			swapChainDesc.Width = m_outputSize.cx;
			swapChainDesc.Height = m_outputSize.cy;

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
		ID3D11Texture2D* frameBuffer = nullptr;
		hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frameBuffer));
		if (FAILED(hr))
		{
			return hr;
		}

		hr = m_d3dDevice->CreateRenderTargetView(frameBuffer, nullptr, &m_d3dRenderTargetView);
		SAFE_RELEASE(frameBuffer);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	// Initializes the viewport.
	m_screenViewport = new D3D11_VIEWPORT[2];

	// For mono output / stereo output (left eye)
	m_screenViewport[0] = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		(FLOAT)m_outputSize.cx,
		(FLOAT)m_outputSize.cy);

	// For stereo output (right eye)
	m_screenViewport[1] = CD3D11_VIEWPORT(
		(FLOAT)m_outputSize.cx,
		0.0f,
		(FLOAT)m_outputSize.cx,
		(FLOAT)m_outputSize.cy);

	return hr;
}

void DeviceResources::SetWindow(HWND hWnd)
{
	CreateWindowSizeDependentResources(hWnd);
}

// Presents the contents of the swap chain to the screen.
void DeviceResources::Present()
{
	m_swapChain->Present(1, 0);
}

// Enables/Disables the stereo output mode.
void DeviceResources::SetStereo(bool enabled)
{
	if (m_isStereo == enabled)
	{
		return;
	}

	m_outputSize.cx = enabled ? m_outputSize.cx << 1 : m_outputSize.cx >> 1;
	m_isStereo = enabled;
}
