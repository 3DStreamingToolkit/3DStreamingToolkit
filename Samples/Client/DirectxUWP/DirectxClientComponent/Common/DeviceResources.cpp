#include "pch.h"

#ifdef HOLOLENS
#include <Collection.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#endif // HOLOLENS

#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Holographic;
using namespace Windows::UI::Core;

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources()
{
#ifndef HOLOLENS
	CreateDeviceResources();
#endif // HOLOLENS
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources()
{
	// Creates the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	// Init device and device context
	D3D11CreateDevice(
#ifdef HOLOLENS
		m_dxgiAdapter.Get(),
#else // HOLOLENS
		nullptr,
#endif // HOLOLENS
		D3D_DRIVER_TYPE_HARDWARE,
		0,
#ifdef HOLOLENS
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
#else // HOLOLENS
		0,
#endif // HOLOLENS
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&context);

	device.As(&m_d3dDevice);
	context.As(&m_d3dContext);

#ifdef HOLOLENS
	// Acquires the DXGI interface for the Direct3D device.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
	);

	// Wrap the native device using a WinRT interop object.
	m_d3dInteropDevice = CreateDirect3DDevice(dxgiDevice.Get());

	// Cache the DXGI adapter.
	// This is for the case of no preferred DXGI adapter, or fallback to WARP.
	ComPtr<IDXGIAdapter> dxgiAdapter;
	DX::ThrowIfFailed(
		dxgiDevice->GetAdapter(&dxgiAdapter)
	);

	DX::ThrowIfFailed(
		dxgiAdapter.As(&m_dxgiAdapter)
	);

	// Enables multithread protection.
	ID3D11Multithread* multithread;
	m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
#endif // HOLOLENS
}

#ifdef HOLOLENS

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
		DX::ThrowIfFailed(
			CreateDXGIFactory2(
				createFlags,
				IID_PPV_ARGS(&dxgiFactory)
			)
		);

		ComPtr<IDXGIFactory4> dxgiFactory4;
		DX::ThrowIfFailed(dxgiFactory.As(&dxgiFactory4));

		// Retrieve the adapter specified by the holographic space.
		DX::ThrowIfFailed(
			dxgiFactory4->EnumAdapterByLuid(
				id,
				IID_PPV_ARGS(&m_dxgiAdapter)
			)
		);
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
			m_d3dContext->DiscardView(pCameraResources->GetBackBufferRenderTargetView());

			// Discard the contents of the depth stencil.
			m_d3dContext->DiscardView(pCameraResources->GetDepthStencilView());
		}
	});
}

#else // HOLOLENS

// These resources need to be recreated every time the window size is changed.
void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// Initialize swapchain
	ComPtr<IDXGIDevice1> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);
	ComPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetAdapter(&dxgiAdapter);
	ComPtr<IDXGIFactory2> dxgiFactory;
	dxgiAdapter->GetParent(_uuidof(IDXGIFactory2), &dxgiFactory);
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2; // Front and back buffer to swap
	swapChainDesc.SampleDesc.Count = 1; // Disable anti-aliasing
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	CoreWindow^ window = CoreWindow::GetForCurrentThread();
	dxgiFactory->CreateSwapChainForCoreWindow(
		m_d3dDevice.Get(),
		reinterpret_cast<IUnknown*>(window),
		&swapChainDesc,
		nullptr,
		&m_swapChain);

	ComPtr<ID3D11Texture2D> frameBuffer;
	m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &frameBuffer);
	m_d3dDevice->CreateRenderTargetView(frameBuffer.Get(), nullptr, &m_d3dRenderTargetView);

	// Initialize viewport
	D3D11_VIEWPORT viewport = { 0 };
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = window->Bounds.Width;
	viewport.Height = window->Bounds.Height;
	m_d3dContext->RSSetViewports(1, &viewport);
}

// This method is called when the CoreWindow is created (or re-created).
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	CreateWindowSizeDependentResources();
}

// Present the contents of the swap chain to the screen.
void DX::DeviceResources::Present()
{
	m_swapChain->Present(1, 0);
}

#endif // HOLOLENS
