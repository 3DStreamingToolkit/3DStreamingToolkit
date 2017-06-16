#pragma once

#ifdef STEREO_OUTPUT_MODE
#include "CameraResources.h"

using namespace Microsoft::WRL;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Windows::Graphics::Holographic;
#endif // STEREO_OUTPUT_MODE

namespace DX
{
	class DeviceResources
	{
	public:
											DeviceResources();
											~DeviceResources();

		void								SetWindow(HWND);
		void								Present();

		// The size of the render target, in pixels.
		SIZE								GetOutputSize() const;

#ifdef STEREO_OUTPUT_MODE
		void								Present(HolographicFrame^ frame);

		// Public methods related to holographic devices.
		void								SetHolographicSpace(HolographicSpace^ space);
		void								EnsureCameraResources(HolographicFrame^ frame, HolographicFramePrediction^ prediction);

		void								AddHolographicCamera(HolographicCamera^ camera);
		void								RemoveHolographicCamera(HolographicCamera^ camera);

		// Holographic accessors.
		template<typename RetType, typename LCallback>
		RetType								UseHolographicCameraResources(const LCallback& callback);
#endif // STEREO_OUTPUT_MODE

		// D3D Accessors.
		ID3D11Device1*						GetD3DDevice() const;
		ID3D11DeviceContext1*				GetD3DDeviceContext() const;
		IDXGISwapChain1*					GetSwapChain() const;
		ID3D11RenderTargetView*				GetBackBufferRenderTargetView() const;
		D3D11_VIEWPORT*						GetScreenViewport() const;
#ifdef STEREO_OUTPUT_MODE
		// DXGI acessors.
		IDXGIAdapter3*						GetDXGIAdapter() const { return m_dxgiAdapter.Get(); }
#endif // STEREO_OUTPUT_MODE

	private:
		// Direct3D objects.
		ID3D11Device1*						m_d3dDevice;
		ID3D11DeviceContext1*				m_d3dContext;
		IDXGISwapChain1*					m_swapChain;

		// Direct3D rendering objects. Required for 3D.
		ID3D11RenderTargetView*				m_d3dRenderTargetView;
		D3D11_VIEWPORT*						m_screenViewport;

		// Cached device properties.
		SIZE								m_outputSize;

#ifdef STEREO_OUTPUT_MODE
		ComPtr<IDXGIAdapter3>				m_dxgiAdapter;

		// Direct3D interop objects.
		IDirect3DDevice^					m_d3dInteropDevice;

		// The holographic space provides a preferred DXGI adapter ID.
		HolographicSpace^					m_holographicSpace;

		// Back buffer resources, etc. for attached holographic cameras.
		std::map<UINT32, std::unique_ptr<CameraResources>> m_cameraResources;
		std::mutex							m_cameraResourcesLock;
#endif // STEREO_OUTPUT_MODE

		HRESULT								CreateDeviceResources();
		void								CleanupResources();
		HRESULT								CreateWindowSizeDependentResources(HWND);
#ifdef STEREO_OUTPUT_MODE
		void								InitializeUsingHolographicSpace();
#endif // STEREO_OUTPUT_MODE
	};
}

#ifdef STEREO_OUTPUT_MODE
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
#endif // STEREO_OUTPUT_MODE
