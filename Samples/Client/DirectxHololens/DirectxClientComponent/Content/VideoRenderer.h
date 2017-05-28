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

#ifdef HOLOLENS
		// Property accessors.
		void SetPosition(Windows::Foundation::Numerics::float3 pos)		{ m_position = pos; }
		Windows::Foundation::Numerics::float3 GetPosition()				{ return m_position; }
#endif // HOLOLENS

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
		ComPtr<ID3D11PixelShader>					m_pixelShader;
		ComPtr<ID3D11GeometryShader>				m_geometryShader;
		ComPtr<ID3D11InputLayout>					m_inputLayout;
		ComPtr<ID3D11ShaderResourceView>			m_textureView;
		ComPtr<ID3D11SamplerState>					m_sampler;

#ifdef HOLOLENS
		ComPtr<ID3D11Buffer>						m_modelConstantBuffer;

		// System resources for geometry.
		ModelConstantBuffer                         m_modelConstantBufferData;

		// Variables used with the rendering loop.
		Windows::Foundation::Numerics::float3		m_position;
#endif // HOLOLENS
	};
}
