#pragma once

namespace DX
{
	class DeviceResources
	{
	public:
		DeviceResources();
		~DeviceResources();
		void SetWindow(HWND);
		void Present();

		// D3D Accessors.
		ID3D11Device1*								GetD3DDevice() const;
		ID3D11DeviceContext1*						GetD3DDeviceContext() const;
		IDXGISwapChain1*							GetSwapChain() const;
		ID3D11RenderTargetView*						GetBackBufferRenderTargetView() const;
		D3D11_VIEWPORT								GetScreenViewport() const;

	private:
		// Direct3D objects.
		ID3D11Device1*								m_d3dDevice;
		ID3D11DeviceContext1*						m_d3dContext;
		IDXGISwapChain1*							m_swapChain;

		// Direct3D rendering objects. Required for 3D.
		ID3D11RenderTargetView*						m_d3dRenderTargetView;
		D3D11_VIEWPORT								m_screenViewport;

		HRESULT CreateDeviceResources();
		HRESULT CreateWindowSizeDependentResources(HWND);
		void CleanupResources();
	};
}