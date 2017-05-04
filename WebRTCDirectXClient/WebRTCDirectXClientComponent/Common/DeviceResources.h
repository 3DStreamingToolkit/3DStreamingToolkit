#pragma once

#ifdef HOLOLENS
#include "CameraResources.h"
#endif // HOLOLENS

namespace DX
{
	class DeviceResources
	{
	public:
		DeviceResources();
#ifdef HOLOLENS
		void Present(Windows::Graphics::Holographic::HolographicFrame^ frame);

		// Public methods related to holographic devices.
		void SetHolographicSpace(Windows::Graphics::Holographic::HolographicSpace^ space);
		void EnsureCameraResources(
			Windows::Graphics::Holographic::HolographicFrame^ frame,
			Windows::Graphics::Holographic::HolographicFramePrediction^ prediction);

		void AddHolographicCamera(Windows::Graphics::Holographic::HolographicCamera^ camera);
		void RemoveHolographicCamera(Windows::Graphics::Holographic::HolographicCamera^ camera);

		// Holographic accessors.
		template<typename RetType, typename LCallback>
		RetType						UseHolographicCameraResources(const LCallback& callback);

#else // HOLOLENS
		void SetWindow(Windows::UI::Core::CoreWindow^ window);
		void Present();
#endif // HOLOLENS

		// D3D Accessors.
		ID3D11Device4*				GetD3DDevice() const					{ return m_d3dDevice.Get(); }
		ID3D11DeviceContext3*		GetD3DDeviceContext() const				{ return m_d3dContext.Get(); }
#ifdef HOLOLENS
		// DXGI acessors.
		IDXGIAdapter3*				GetDXGIAdapter() const					{ return m_dxgiAdapter.Get(); }
#else // HOLOLENS
		IDXGISwapChain1*			GetSwapChain() const					{ return m_swapChain.Get(); }
		ID3D11RenderTargetView*		GetBackBufferRenderTargetView() const	{ return m_d3dRenderTargetView.Get(); }
		D3D11_VIEWPORT				GetScreenViewport() const				{ return m_screenViewport; }
#endif // HOLOLENS

	private:
		void CreateDeviceResources();
#ifdef HOLOLENS
		void InitializeUsingHolographicSpace();
#else // HOLOLENS
		void CreateWindowSizeDependentResources();
#endif // HOLOLENS

		// Direct3D objects.
		Microsoft::WRL::ComPtr<ID3D11Device4>						m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext3>				m_d3dContext;
#ifdef HOLOLENS
		Microsoft::WRL::ComPtr<IDXGIAdapter3>						m_dxgiAdapter;

		// Direct3D interop objects.
		Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice^	m_d3dInteropDevice;

		// The holographic space provides a preferred DXGI adapter ID.
		Windows::Graphics::Holographic::HolographicSpace^			m_holographicSpace;

		// Back buffer resources, etc. for attached holographic cameras.
		std::map<UINT32, std::unique_ptr<CameraResources>>			m_cameraResources;
		std::mutex													m_cameraResourcesLock;
#else // HOLOLENS
		Microsoft::WRL::ComPtr<IDXGISwapChain1>						m_swapChain;

		// Direct3D rendering objects. Required for 3D.
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>				m_d3dRenderTargetView;
		D3D11_VIEWPORT												m_screenViewport;
#endif // HOLOLENS
	};
}

#ifdef HOLOLENS
// Device-based resources for holographic cameras are stored in a std::map.
// Access this list by providing a callback to this function, and the std::map
// will be guarded from add and remove events until the callback returns.
// The callback is processed immediately and must not contain any nested calls
// to UseHolographicCameraResources.
// The callback takes a parameter of type
// std::map<UINT32,std::unique_ptr<DX::CameraResources>>& through which the list
// of cameras will be accessed.
template<typename RetType, typename LCallback>
RetType DX::DeviceResources::UseHolographicCameraResources(const LCallback& callback)
{
	std::lock_guard<std::mutex> guard(m_cameraResourcesLock);
	return callback(m_cameraResources);
}
#endif // HOLOLENS
