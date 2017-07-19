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
			ID3D11ShaderResourceView* textureView);

		void CreateDeviceDependentResources();

		void ReleaseDeviceDependentResources();

		void Update(DX::StepTimer timer);

		void Render();

		// Property accessors.
		void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos; }

		Windows::Foundation::Numerics::float3 GetPosition() { return m_position; }

	private:
		std::shared_ptr<DX::DeviceResources>		m_deviceResources;

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
