
#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt\Windows.Graphics.Display.h>

using namespace D2D1;
using namespace Microsoft::WRL;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::Graphics::Holographic;

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_holographicSpace(nullptr),
	m_supportsVprt(false)
{
	CreateDeviceIndependentResources();
}

// Configures resources that don't depend on the Direct3D device.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Initialize Direct2D resources.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// If the project is in a debug build, enable Direct2D debugging via SDK Layers.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Initialize the Direct2D Factory.
	winrt::check_hresult(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
		)
	);

	// Initialize the DirectWrite Factory.
	winrt::check_hresult(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
		)
	);
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources()
{
	// Creates the Direct3D 11 API device object and a corresponding context.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	// Init device and device context
	D3D11CreateDevice(
		m_dxgiAdapter.Get(),
		D3D_DRIVER_TYPE_HARDWARE,
		0,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&context);

	device.As(&m_d3dDevice);
	context.As(&m_d3dContext);

	// Acquires the DXGI interface for the Direct3D device.
	ComPtr<IDXGIDevice3> dxgiDevice;
	winrt::check_hresult(
		m_d3dDevice.As(&dxgiDevice)
	);

	// Wrap the native device using a WinRT interop object.
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), m_d3dInteropDevice.as<IInspectable>().put()));
	//m_d3dInteropDevice = CreateDirect3DDevice(dxgiDevice.Get());
	

	// Cache the DXGI adapter.
	// This is for the case of no preferred DXGI adapter, or fallback to WARP.
	ComPtr<IDXGIAdapter> dxgiAdapter;
	winrt::check_hresult(
		dxgiDevice->GetAdapter(&dxgiAdapter)
	);

	winrt::check_hresult(
		dxgiAdapter.As(&m_dxgiAdapter)
	);

	winrt::check_hresult(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
	);

	winrt::check_hresult(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
		)
	);

	// Enables multithread protection.
	ID3D11Multithread* multithread;
	m_d3dDevice->QueryInterface(IID_PPV_ARGS(&multithread));
	multithread->SetMultithreadProtected(true);
	multithread->Release();
}

void DX::DeviceResources::SetHolographicSpace(HolographicSpace const& holographicSpace)
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
		m_holographicSpace.PrimaryAdapterId().LowPart,
		m_holographicSpace.PrimaryAdapterId().HighPart
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
		winrt::check_hresult(
			CreateDXGIFactory2(
				createFlags,
				IID_PPV_ARGS(&dxgiFactory)
			)
		);

		ComPtr<IDXGIFactory4> dxgiFactory4;
		winrt::check_hresult(dxgiFactory.As(&dxgiFactory4));

		// Retrieve the adapter specified by the holographic space.
		winrt::check_hresult(
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

	m_holographicSpace.SetDirect3D11Device(m_d3dInteropDevice);

	// Check for device support for the optional feature that allows setting
	// the render target array index from the vertex shader stage.
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
	m_d3dDevice->CheckFeatureSupport(
		D3D11_FEATURE_D3D11_OPTIONS3,
		&options,
		sizeof(options));

	if (options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer)
	{
		m_supportsVprt = true;
	}
}

// Validates the back buffer for each HolographicCamera and recreates
// resources for back buffers that have changed.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::EnsureCameraResources(
	HolographicFrame const& frame,
	HolographicFramePrediction const& prediction)
{
	UseHolographicCameraResources<void>([this, frame, prediction](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		for (HolographicCameraPose pose : prediction.CameraPoses())
		{
			HolographicCameraRenderingParameters renderingParameters = frame.GetRenderingParameters(pose);
			CameraResources* pCameraResources = cameraResourceMap[pose.HolographicCamera().Id()].get();

			pCameraResources->CreateResourcesForBackBuffer(this, renderingParameters);
		}
	});
}

// Prepares to allocate resources and adds resource views for a camera.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::AddHolographicCamera(HolographicCamera const& camera)
{
	UseHolographicCameraResources<void>([this, camera](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		cameraResourceMap[camera.Id()] = std::make_unique<CameraResources>(camera);
	});

	m_d3dRenderTargetSize = camera.RenderTargetSize();
}

// Deallocates resources for a camera and removes the camera from the set.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::RemoveHolographicCamera(HolographicCamera const& camera)
{
	UseHolographicCameraResources<void>([this, camera](std::map<UINT32, std::unique_ptr<CameraResources>>& cameraResourceMap)
	{
		CameraResources* pCameraResources = cameraResourceMap[camera.Id()].get();

		if (pCameraResources != nullptr)
		{
			pCameraResources->ReleaseResourcesForBackBuffer(this);
			cameraResourceMap.erase(camera.Id());
		}
	});
}

// Present the contents of the swap chain to the screen.
// Locks the set of holographic camera resources until the function exits.
void DX::DeviceResources::Present(HolographicFrame const& frame)
{
	// By default, this API waits for the frame to finish before it returns.
	// Holographic apps should wait for the previous frame to finish before
	// starting work on a new frame. This allows for better results from
	// holographic frame predictions.
	HolographicFramePresentResult presentResult = frame.PresentUsingCurrentPrediction(HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish);
}
