#pragma once

#include "DeviceResources.h"
#include "ShaderStructures.h"

using namespace Microsoft::WRL;

namespace DirectXClientComponent
{
	class VideoRenderer
	{
	public:
		VideoRenderer(
			const std::shared_ptr<DX::DeviceResources>& deviceResources,
			int width,
			int height);

		void										CreateDeviceDependentResources();
		void										ReleaseDeviceDependentResources();
		void										Render();

		// Property accessors.
		ID3D11Texture2D*							GetVideoFrame()			{ return m_videoFrame.Get();	}
		Windows::Foundation::Numerics::float3		GetFocusPoint()			{ return m_focusPoint;			}
		void SetFocusPoint(Windows::Foundation::Numerics::float3 pos)		{ m_focusPoint = pos;			}

	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;
		int											m_width;
		int											m_height;

		// Direct3D resources for geometry.
		ComPtr<ID3D11Buffer>						m_vertexBuffer;
		ComPtr<ID3D11VertexShader>					m_vertexShader;
		ComPtr<ID3D11GeometryShader>				m_geometryShader;
		ComPtr<ID3D11PixelShader>					m_pixelShader;
		ComPtr<ID3D11InputLayout>					m_inputLayout;
		ComPtr<ID3D11Texture2D>						m_videoFrame;
		ComPtr<ID3D11ShaderResourceView>			m_textureView;
		ComPtr<ID3D11SamplerState>					m_sampler;

		// If the current D3D Device supports VPRT, we can avoid using a geometry
		// shader just to set the render target array index.
		bool                                        m_usingVprtShaders;

		Windows::Foundation::Numerics::float3       m_focusPoint;
	};
}
