#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Platform;

// Constructor for DeviceResources.
DX::DeviceResources::DeviceResources()
{
	CreateDeviceResources();
}

// Configures the Direct3D device, and stores handles to it and the device context.
void DX::DeviceResources::CreateDeviceResources()
{
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;

	// Init device and device context
	D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&deviceContext);

	device.As(&m_d3dDevice);
	deviceContext.As(&m_d3dContext);
}

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
