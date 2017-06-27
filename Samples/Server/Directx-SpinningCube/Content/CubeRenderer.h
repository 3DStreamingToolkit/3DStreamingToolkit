#pragma once

#include "DeviceResources.h"

namespace Toolkit3DSample
{
	// Constant buffer used to send model matrix to the vertex shader.
	struct ModelConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
	};

	// Constant buffer used to send view matrix to the vertex shader.
	struct ViewConstantBuffer
	{
		DirectX::XMFLOAT4X4 view;
	};

	// Constant buffer used to send projection matrix to the vertex shader.
	struct ProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 projection;
	};

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
		void									Update();
		void									Render();
		void									UpdateView(const DirectX::XMFLOAT4X4& viewProjectionLeft, const DirectX::XMFLOAT4X4& viewProjectionRight);
		void									UpdateView(const DirectX::XMVECTORF32& eye, const DirectX::XMVECTORF32& lookAt, const DirectX::XMVECTORF32& up);

	private:
		// Cached pointer to device resources.
		DX::DeviceResources*					m_deviceResources;

		// Direct3D resources for cube geometry.
		ID3D11Buffer*							m_vertexBuffer;
		ID3D11Buffer*							m_indexBuffer;
		ID3D11Buffer*							m_modelConstantBuffer;
		ID3D11Buffer*							m_viewConstantBuffer;
		ID3D11Buffer*							m_projectionConstantBuffer;
		ID3D11VertexShader*						m_vertexShader;
		ID3D11PixelShader*						m_pixelShader;
		ID3D11InputLayout*						m_inputLayout;

		// System resources for cube geometry.
		ModelConstantBuffer						m_modelConstantBufferData;
		ViewConstantBuffer						m_viewConstantBufferData;
		ProjectionConstantBuffer				m_projectionConstantBufferData[2];
		uint32_t								m_indexCount;

		// Variables used with the rendering loop.
		float									m_degreesPerSecond;
	};
}
