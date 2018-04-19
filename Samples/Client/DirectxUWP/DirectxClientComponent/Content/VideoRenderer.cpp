#include "pch.h"
#include "VideoRenderer.h"
#include "DirectXHelper.h"

#ifdef SHOW_DEBUG_INFO
#define FONT_SIZE		40.0f
#define TEXT_INDENT		20
#endif // SHOW_DEBUG_INFO

using namespace DirectX;
using namespace DirectXClientComponent;
using namespace Platform;

VideoRenderer::VideoRenderer(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	int width,
	int height) :
		m_deviceResources(deviceResources),
		m_width(width),
		m_height(height),
		m_usingVprtShaders(false)
{
	// Sets a fixed focus point two meters in front of user for image stabilization.
	m_focusPoint = { 0.0f, 0.0f, -2.0f };

#ifdef SHOW_DEBUG_INFO
	// Creates text format.
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_LIGHT,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			FONT_SIZE,
			L"en-US",
			&m_textFormat
		)
	);
#endif // SHOW_DEBUG_INFO

	CreateDeviceDependentResources();
}

void VideoRenderer::CreateDeviceDependentResources()
{
	int width = m_deviceResources->GetRenderTargetSize().Width;
	int height = m_deviceResources->GetRenderTargetSize().Height;

	VertexPositionTexture vertices[] =
	{
#ifdef UNITY_UV_STARTS_AT_TOP
		// Left camera.
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 0.0f) },
		{ XMFLOAT3(0.0f,	0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(width, height, 0.0f), XMFLOAT3(0.5f, 1.0f, 0.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 0.0f) },
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

		// Right camera
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.5f, 1.0f, 1.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.0f,	0.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(width, height, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.5f, 1.0f, 1.0f) }
#else // UNITY_UV_STARTS_AT_TOP
		// Left camera.
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.0f,	0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(width, height, 0.0f), XMFLOAT3(0.5f, 0.0f, 0.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },

		// Right camera
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(0.0f,	0.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 1.0f) },
		{ XMFLOAT3(width, height, 0.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(width,	0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(0.0f, height, 0.0f), XMFLOAT3(0.5f, 0.0f, 1.0f) }
#endif // UNITY_UV_STARTS_AT_TOP
	};

	D3D11_BUFFER_DESC bufferDesc = { 0 };
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(VertexPositionTexture) * ARRAYSIZE(vertices);
	D3D11_SUBRESOURCE_DATA subresourceData = { vertices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&bufferDesc, &subresourceData, &m_vertexBuffer);

	// Initializes the video texture.
	D3D11_TEXTURE2D_DESC videoTextureDesc = { 0 };
	videoTextureDesc.ArraySize = 1;
	videoTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	videoTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	videoTextureDesc.Width = m_width;
	videoTextureDesc.Height = m_height;
	videoTextureDesc.MipLevels = 1;
	videoTextureDesc.SampleDesc.Count = 1;
	videoTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	m_deviceResources->GetD3DDevice()->CreateTexture2D(
		&videoTextureDesc, nullptr, &m_videoFrame);

	// Creates the shader resource view so that shaders may use it.
	D3D11_SHADER_RESOURCE_VIEW_DESC textureViewDesc;
	ZeroMemory(&textureViewDesc, sizeof(textureViewDesc));
	textureViewDesc.Format = videoTextureDesc.Format;
	textureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	textureViewDesc.Texture2D.MipLevels = videoTextureDesc.MipLevels;
	textureViewDesc.Texture2D.MostDetailedMip = 0;

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
			m_videoFrame.Get(),
			&textureViewDesc,
			&m_textureView
		)
	);

	// Creates the sampler.
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));

	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

	// The sampler does not use anisotropic filtering, so this parameter is ignored.
	samplerDesc.MaxAnisotropy = 0;

	// Specify how texture coordinates outside of the range 0..1 are resolved.
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	// Use no special MIP clamping or bias.
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Don't use a comparison function.
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	// Border address mode is not used, so this parameter is ignored.
	samplerDesc.BorderColor[0] = 0.0f;
	samplerDesc.BorderColor[1] = 0.0f;
	samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 0.0f;

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateSamplerState(
			&samplerDesc,
			&m_sampler
		)
	);

	m_deviceResources->GetD3DDeviceContext()->PSSetShaderResources(
		0,
		1,
		m_textureView.GetAddressOf()
	);

	m_deviceResources->GetD3DDeviceContext()->PSSetSamplers(
		0,
		1,
		m_sampler.GetAddressOf()
	);

	auto loadVertexShaderTask = DX::ReadDataAsync(
		m_usingVprtShaders ? L"VprtVertexShader.cso" : L"VertexShader.cso");

	loadVertexShaderTask.then([this](std::vector<byte> file)
	{
		m_deviceResources->GetD3DDevice()->CreateVertexShader(
			file.data(), file.size(), nullptr, &m_vertexShader);

		D3D11_INPUT_ELEMENT_DESC elementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		m_deviceResources->GetD3DDevice()->CreateInputLayout(
			elementDesc,
			ARRAYSIZE(elementDesc),
			file.data(),
			file.size(),
			&m_inputLayout);

		m_deviceResources->GetD3DDeviceContext()->IASetInputLayout(m_inputLayout.Get());
		m_deviceResources->GetD3DDeviceContext()->VSSetShader(
			m_vertexShader.Get(), nullptr, 0);
	});

	auto loadPixelShaderTask = DX::ReadDataAsync(L"PixelShader.cso");
	loadPixelShaderTask.then([this](std::vector<byte> file)
	{
		m_deviceResources->GetD3DDevice()->CreatePixelShader(
			file.data(), file.size(), nullptr, &m_pixelShader);

		m_deviceResources->GetD3DDeviceContext()->PSSetShader(
			m_pixelShader.Get(), nullptr, 0);
	});

	if (!m_usingVprtShaders)
	{
		// After the pass-through geometry shader file is loaded, create the shader.
		auto loadGeometryShaderTask = DX::ReadDataAsync(L"GeometryShader.cso").then(
			[this](const std::vector<byte>& fileData)
		{
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateGeometryShader(
					fileData.data(),
					fileData.size(),
					nullptr,
					&m_geometryShader
				)
			);

			// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
			// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
			// a pass-through geometry shader is used to set the render target 
			// array index.
			m_deviceResources->GetD3DDeviceContext()->GSSetShader(
				m_geometryShader.Get(),
				nullptr,
				0
			);
		});
	}

#ifdef SHOW_DEBUG_INFO
	// Obtains a DXGI surface.
	ComPtr<IDXGISurface> dxgiSurface;
	DX::ThrowIfFailed(m_videoFrame->QueryInterface(dxgiSurface.GetAddressOf()));

	// Creates a D2D render target.
	D2D1_RENDER_TARGET_PROPERTIES props =
		D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		);

	DX::ThrowIfFailed(m_deviceResources->GetD2DFactory()->CreateDxgiSurfaceRenderTarget(
		dxgiSurface.Get(),
		&props,
		&m_d2dRenderTarget
	));

	m_d2dRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White, 1.0f),
		&m_brush
	);
#endif // SHOW_DEBUG_INFO
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	m_usingVprtShaders = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexBuffer.Reset();
	m_geometryShader.Reset();
	m_brush.Reset();
}

void VideoRenderer::Render(int fps, int latency)
{
#ifdef SHOW_DEBUG_INFO
	String^ debugInfo = L"FPS: " + fps + L" Latency: " + latency;
	int halfWidth = m_width >> 1;

	D2D1_RECT_F leftTextRect = 
	{
		TEXT_INDENT,
		0,
		halfWidth,
		m_height
	};

	D2D1_RECT_F rightTextRect = 
	{
		halfWidth + TEXT_INDENT,
		0,
		m_width,
		m_height
	};

	m_d2dRenderTarget->BeginDraw();

#ifdef UNITY_UV_STARTS_AT_TOP
	m_d2dRenderTarget->SetTransform(
		D2D1::Matrix3x2F::Scale(
			1.0f,
			-1.0f,
			D2D1::Point2F(halfWidth, m_height >> 1))
	);
#endif // UNITY_UV_STARTS_AT_TOP

	m_d2dRenderTarget->DrawText(
		debugInfo->Data(),
		debugInfo->Length(),
		m_textFormat.Get(),
		leftTextRect,
		m_brush.Get());

#ifdef UNITY_UV_STARTS_AT_TOP
	m_d2dRenderTarget->SetTransform(
		D2D1::Matrix3x2F::Scale(
			1.0f,
			-1.0f,
			D2D1::Point2F(m_width + halfWidth, m_height >> 1))
	);
#endif // UNITY_UV_STARTS_AT_TOP

	m_d2dRenderTarget->DrawText(
		debugInfo->Data(),
		debugInfo->Length(),
		m_textFormat.Get(),
		rightTextRect,
		m_brush.Get());

	m_d2dRenderTarget->EndDraw();
#endif // SHOW_DEBUG_INFO

	// Gets the device context.
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

	// Rendering.
	UINT stride = sizeof(VertexPositionTexture);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->Draw(12, 0);
}
