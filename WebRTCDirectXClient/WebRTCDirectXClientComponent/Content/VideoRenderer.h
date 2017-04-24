#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

using namespace Microsoft::WRL;

struct BasicVertex
{
	DirectX::XMFLOAT3 position;		// Position
	DirectX::XMFLOAT2 textureUV;	// Texture coordinate
};

namespace WebRTCDirectXClientComponent
{
	class VideoRenderer
	{
	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;
		int											m_width;
		int											m_height;
		ID3D11Texture2D*							m_videoTexture;
		D3D11_TEXTURE2D_DESC						m_videoTextureDesc;

		// Direct3D resources for geometry.
		ComPtr<ID3D11Buffer>						m_vertexBuffer;
		ComPtr<ID3D11VertexShader>					m_vertexShader;
		ComPtr<ID3D11PixelShader>					m_pixelShader;
		ComPtr<ID3D11InputLayout>					m_inputLayout;
		ComPtr<ID3D11ShaderResourceView>			m_textureView;
		ComPtr<ID3D11SamplerState>					m_sampler;

	public:
		VideoRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources, int width, int height);
		
		void InitGraphics();
		
		void InitPipeline();

		void Render(const uint8_t* data);
	};
}
