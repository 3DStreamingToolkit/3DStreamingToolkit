#include "pch.h"
#ifdef STEREO_OUTPUT_MODE
#include <Collection.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "dxgi")
#endif // STEREO_OUTPUT_MODE
#include "DeviceResources.h"
#include "macros.h"

using namespace DX;

// Constructor for DeviceResources.
DeviceResources::DeviceResources()
{
#ifndef STEREO_OUTPUT_MODE
	CreateDeviceResources();
#endif // STEREO_OUTPUT_MODE
}

// Destructor for DeviceResources.
DeviceResources::~DeviceResources()
{
	CleanupResources();
}

void DeviceResources::CleanupResources()
{
	delete []m_screenViewport;
	SAFE_RELEASE(m_d3dRenderTargetView);
	SAFE_RELEASE(m_d3dContext);
	SAFE_RELEASE(m_d3dDevice);
	SAFE_RELEASE(m_swapChain);
}

SIZE DeviceResources::GetOutputSize() const
{
	return m_outputSize;
}

ID3D11RenderTargetView* DeviceResources::GetBackBufferRenderTargetView() const
{
	return m_d3dRenderTargetView;
}

D3D11_VIEWPORT* DeviceResources::GetScreenViewport() const
{
	return m_screenViewport;
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

// Configures the Direct3D device, and stores handles to it and the device context.
HRESULT DeviceResources::CreateDeviceResources()
{
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	HRESULT hr = S_OK;
#ifdef STEREO_OUTPUT_MODE
	UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#else // STEREO_OUTPUT_MODE
	UINT createDeviceFlags = 0;
#endif // STEREO_OUTPUT_MODE

	// Creates D3D11 device.
	hr = D3D11CreateDevice(
#ifdef STEREO_OUTPUT_MODE
		m_dxgiAdapter.Get(),
#else // STEREO_OUTPUT_MODE
		nullptr,
#endif // STEREO_OUTPUT_MODE
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

#ifdef STEREO_OUTPUT_MODE
	// Acquires the DXGI interface for the Direct3D device.
	IDXGIDevice* dxgiDevice = nullptr;
	hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
	if (SUCCEEDED(hr))
	{
		// Wrap the native device using a WinRT interop object.
		m_d3dInteropDevice = CreateDirect3DDevice(dxgiDevice);

		// Cleanup.
		dxgiDevice->Release();
	}

	// Enables multithread protection.
	ID3D11Multithread* multithread;
	m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // STEREO_OUTPUT_MODE

	return hr;
}

// These resources need to be recreated every time the window size is changed.
HRESULT DeviceResources::CreateWindowSizeDependentResources(HWND hWnd)
{
	// Obtains DXGI factory from device.
	HRESULT hr = S_OK;
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
#ifdef STEREO_OUTPUT_MODE
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
#else // STEREO_OUTPUT_MODE
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif // STEREO_OUTPUT_MODE
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

	// Initializes the viewport.
#ifndef STEREO_OUTPUT_MODE
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
	m_swapChain->Present(1, 0);
}

#ifdef STEREO_OUTPUT_MODE

void DX::DeviceResources::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
	// Cache the holographic space. Used to re-initalize during device-lost scenarios.
	m_holographicSpace = holographicSpace;

	InitializeUsingHolographicSpace();
}

void DX::DeviceResources::InitializeUsingHolographicSpace()
{
	// The holographic space might need to determine which adapter supports
	// holograms, in which case it will specify a non-zero PrimaryAdapterId.
	LUID id =
	{
		m_holographicSpace->PrimaryAdapterId.LowPart,
		m_holographicSpace->PrimaryAdapterId.HighPart
	};

	// When a primary adapter ID is given to the app, the app should find
	// the corresponding DXGI adapter and use it to create Direct3D devices
	// and device contexts. Otherwise, there is no restriction on the DXGI
	// adapter the app can use.
	if ((id.HighPart != 0) && (id.LowPart != 0))
	{
		UINT createFlags = 0;
#ifdef DEBUG
		if (DX::SdkLayersAvailable())
		{
			createFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif
		// Create the DXGI factory.
		ComPtr<IDXGIFactory1> dxgiFactory;
		HRESULT hr = CreateDXGIFactory2(createFlags, IID_PPV_ARGS(&dxgiFactory));
		if (hr == S_OK)
		{
			ComPtr<IDXGIFactory4> dxgiFactory4;
			if (SUCCEEDED(dxgiFactory.As(&dxgiFactory4)))
			{
				// Retrieve the adapter specified by the holographic space.
				dxgiFactory4->EnumAdapterByLuid(
					id,
					IID_PPV_ARGS(&m_dxgiAdapter)
				);
			}		
		}
	}
	else
	{
		m_dxgiAdapter.Reset();
	}

	CreateDeviceResources();

	m_holographicSpace->SetDirect3D11Device(m_d3dInteropDevice);
}

// Validates the back buffer for each HolographicCamera and recreates
// resources for back buffers that have changed.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::EnsureCameraResources(
	HolographicFrame^ frame,
	HolographicFramePrediction^ prediction)
{
	UseHolographicCameraResources<void>([this, frame, prediction](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		for (HolographicCameraPose^ pose : prediction->CameraPoses)
		{
			HolographicCameraRenderingParameters^ renderingParameters = frame->GetRenderingParameters(pose);
			CameraResources* pCameraResources = cameraResourceMap[pose->HolographicCamera->Id].get();

			pCameraResources->CreateResourcesForBackBuffer(this, renderingParameters);
		}
	});
}

// Prepares to allocate resources and adds resource views for a camera.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::AddHolographicCamera(HolographicCamera^ camera)
{
	UseHolographicCameraResources<void>([this, camera](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		cameraResourceMap[camera->Id] = std::make_unique<CameraResources>(camera);
	});
}

// Deallocates resources for a camera and removes the camera from the set.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::RemoveHolographicCamera(HolographicCamera^ camera)
{
	UseHolographicCameraResources<void>([this, camera](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		CameraResources* pCameraResources = cameraResourceMap[camera->Id].get();

		if (pCameraResources != nullptr)
		{
			pCameraResources->ReleaseResourcesForBackBuffer(this);
			cameraResourceMap.erase(camera->Id);
		}
	});
}

// Present the contents of the swap chain to the screen.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::Present(HolographicFrame^ frame)
{
	// By default, this API waits for the frame to finish before it returns.
	// Holographic apps should wait for the previous frame to finish before
	// starting work on a new frame. This allows for better results from
	// holographic frame predictions.
	HolographicFramePresentResult presentResult = frame->PresentUsingCurrentPrediction();

	HolographicFramePrediction^ prediction = frame->CurrentPrediction;
	UseHolographicCameraResources<void>([this, prediction](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		for (auto cameraPose : prediction->CameraPoses)
		{
			// This represents the device-based resources for a HolographicCamera.
			DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose->HolographicCamera->Id].get();

			// Discard the contents of the render target.
			// This is a valid operation only when the existing contents will be
			// entirely overwritten. If dirty or scroll rects are used, this call
			// should be removed.
			if (pCameraResources != nullptr)
			{
				m_d3dContext->DiscardView(pCameraResources->GetBackBufferRenderTargetView());
			}
		}
	});
}

#endif // STEREO_OUTPUT_MODE

