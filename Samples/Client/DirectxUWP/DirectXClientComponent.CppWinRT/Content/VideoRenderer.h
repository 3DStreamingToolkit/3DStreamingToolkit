#pragma once
#include "Common\DeviceResources.h"
#include "ShaderStructures.h"
#include <winrt\Windows.Foundation.Numerics.h>
#include <wrl\client.h>

#define SHOW_DEBUG_INFO
//#define UNITY_UV_STARTS_AT_TOP

namespace DirectXClientComponent_CppWinRT
{
	class VideoRenderer
	{
	public:
		VideoRenderer(
			const std::shared_ptr<DX::DeviceResources>& deviceResources,
			int width,
			int height);

		winrt::Windows::Foundation::IAsyncAction	CreateDeviceDependentResources();
		void										ReleaseDeviceDependentResources();
		void										Render(int fps = 0, int latency = 0);

		// Property accessors.
		ID3D11Texture2D*							GetVideoFrame() { return m_videoFrame.Get(); }
		winrt::Windows::Foundation::Numerics::float3		GetFocusPoint() { return m_focusPoint; }
		void SetFocusPoint(winrt::Windows::Foundation::Numerics::float3 pos) { m_focusPoint = pos; }

	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;
		int											m_width;
		int											m_height;

		// Direct3D resources for geometry.
		Microsoft::WRL::ComPtr<ID3D11Buffer>						m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>					m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader>				m_geometryShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>					m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>					m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>						m_videoFrame;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>			m_textureView;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>					m_sampler;

		// If the current D3D Device supports VPRT, we can avoid using a geometry
		// shader just to set the render target array index.
		bool                                        m_usingVprtShaders;

		winrt::Windows::Foundation::Numerics::float3       m_focusPoint;

		// Resources related to text rendering.
		Microsoft::WRL::ComPtr<ID2D1RenderTarget>					m_d2dRenderTarget;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>				m_brush;
		Microsoft::WRL::ComPtr<IDWriteTextFormat>					m_textFormat;
	};
}
