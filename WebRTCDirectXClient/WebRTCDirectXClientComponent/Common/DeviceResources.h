#pragma once

namespace DX
{
	class DeviceResources
	{
	public:
		DeviceResources();
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void Present();

		// D3D Accessors.
		ID3D11Device1*				GetD3DDevice() const { return m_d3dDevice.Get(); }
		ID3D11DeviceContext1*		GetD3DDeviceContext() const { return m_d3dContext.Get(); }
		IDXGISwapChain1*			GetSwapChain() const { return m_swapChain.Get(); }
		ID3D11RenderTargetView*		GetBackBufferRenderTargetView() const { return m_d3dRenderTargetView.Get(); }
		D3D11_VIEWPORT				GetScreenViewport() const { return m_screenViewport; }

	private:
		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D11Device1>			m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1>	m_d3dContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain1>			m_swapChain;

		// Direct3D rendering objects. Required for 3D.
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	m_d3dRenderTargetView;
		D3D11_VIEWPORT									m_screenViewport;

		void CreateDeviceResources();
		void CreateWindowSizeDependentResources();
	};
}