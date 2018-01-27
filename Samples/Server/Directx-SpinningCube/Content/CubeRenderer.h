#pragma once

#include "DeviceResources.h"

// Eye is at (0, 0, 1), looking at point (0, 0, 0) with the up-vector along the y-axis.
static const DirectX::XMVECTORF32 kEye = { 0.0f, 0.0f, 1.0f, 0.0f };
static const DirectX::XMVECTORF32 kLookAt = { 0.0f, 0.0f, 0.0f, 0.0f };
static const DirectX::XMVECTORF32 kUp = { 0.0f, 1.0f, 0.0f, 0.0f };

namespace StreamingToolkitSample
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

		void									UpdateView(const DirectX::XMFLOAT4X4& viewProjectionLeft, const DirectX::XMFLOAT4X4& viewProjectionRight);
		void									UpdateView(const DirectX::XMVECTORF32& eye, const DirectX::XMVECTORF32& lookAt, const DirectX::XMVECTORF32& up);
		void									Render(ID3D11RenderTargetView* renderTargetView = nullptr);

		// Property accessors.
		void									SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos; }
		Windows::Foundation::Numerics::float3	GetPosition() { return m_position; }
		DirectX::XMVECTORF32					GetDefaultEyeVector() { return kEye; }
		DirectX::XMVECTORF32					GetDefaultLookAtVector() { return kLookAt; }
		DirectX::XMVECTORF32					GetDefaultUpVector() { return kUp; }

	private:
		void									InitGraphics();
		void									InitPipeline();
		void									InternalUpdate();

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
		Windows::Foundation::Numerics::float3   m_position = { 0.f, 0.f, 0.f };
	};
}
