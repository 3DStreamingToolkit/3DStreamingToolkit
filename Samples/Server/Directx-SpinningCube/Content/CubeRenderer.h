#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

namespace Toolkit3DSample
{
#ifdef STEREO_OUTPUT_MODE
	// Constant buffer used to send hologram position transform to the shader pipeline.
	struct ModelConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
	};
#else
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};
#endif // STEREO_OUTPUT_MODE

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
	};

	class CubeRenderer
	{
	public:
												CubeRenderer(DX::DeviceResources* deviceResources);
		void									InitGraphics();
		void									InitPipeline();
		void									Update(const DX::StepTimer& timer);
		void									Render();

#ifdef STEREO_OUTPUT_MODE
		// Repositions the sample hologram.
		void PositionHologram(Windows::UI::Input::Spatial::SpatialPointerPose^ pointerPose);

		// Repositions the sample hologram, using direct measures.
		void PositionHologram(Windows::Foundation::Numerics::float3 pos, Windows::Foundation::Numerics::float3 dir);

		// Property accessors.
		void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos; }
		Windows::Foundation::Numerics::float3 GetPosition() { return m_position; }
#endif // STEREO_OUTPUT_MODE

	private:
		// Cached pointer to device resources.
		DX::DeviceResources*					m_deviceResources;

		// Direct3D resources for cube geometry.
		ID3D11Buffer*							m_vertexBuffer;
		ID3D11Buffer*							m_indexBuffer;
		ID3D11Buffer*							m_constantBuffer;
		ID3D11VertexShader*						m_vertexShader;
		ID3D11PixelShader*						m_pixelShader;
		ID3D11GeometryShader*					m_geometryShader;
		ID3D11InputLayout*						m_inputLayout;

		// System resources for cube geometry.
#ifdef STEREO_OUTPUT_MODE
		ModelConstantBuffer						m_constantBufferData;
#else // STEREO_OUTPUT_MODE
		ModelViewProjectionConstantBuffer		m_constantBufferData;
#endif // STEREO_OUTPUT_MODE
		uint32_t								m_indexCount;

		// Variables used with the rendering loop.
		float                                   m_degreesPerSecond = 45.f;
		Windows::Foundation::Numerics::float3   m_position = { 0.f, 0.f, -2.f };
	};
}
