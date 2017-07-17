#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ShaderStructures.h"

using namespace Microsoft::WRL;

namespace DirectXClientComponent
{
	class VideoRenderer
	{
	public:
		VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, 
			int width, int height);

		void CreateDeviceDependentResources();

		void ReleaseDeviceDependentResources();

		void Update(DX::StepTimer timer);

		void Render();

		void UpdateFrame(const uint8_t* data);

		// Property accessors.
		void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos; }

		Windows::Foundation::Numerics::float3 GetPosition() { return m_position; }

		int GetWidth() { return m_width; }

		int GetHeight() { return m_height; }

		ID3D11Texture2D** GetVideoTexture() { return &m_videoTexture; }

	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;
		int											m_width;
		int											m_height;
		const uint8_t*								m_frameData;
		ID3D11Texture2D*							m_videoTexture;
		D3D11_TEXTURE2D_DESC						m_videoTextureDesc;

		// Direct3D resources for geometry.
		ComPtr<ID3D11Buffer>						m_vertexBuffer;
		ComPtr<ID3D11VertexShader>					m_vertexShader;
		ComPtr<ID3D11GeometryShader>				m_geometryShader;
		ComPtr<ID3D11PixelShader>					m_pixelShader;
		ComPtr<ID3D11InputLayout>					m_inputLayout;
		ComPtr<ID3D11ShaderResourceView>			m_textureView;
		ComPtr<ID3D11SamplerState>					m_sampler;

		// Variables used with the rendering loop.
		Windows::Foundation::Numerics::float3       m_position;

		// If the current D3D Device supports VPRT, we can avoid using a geometry
		// shader just to set the render target array index.
		bool                                        m_usingVprtShaders;
	};
}
