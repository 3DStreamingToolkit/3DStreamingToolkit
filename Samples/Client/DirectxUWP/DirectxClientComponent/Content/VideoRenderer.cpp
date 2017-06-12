#include "pch.h"
#include "VideoRenderer.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace Windows::UI::Core;
using namespace DirectXClientComponent;

VideoRenderer::VideoRenderer(
	const std::shared_ptr<DX::DeviceResources>& deviceResources,
	int width,
	int height) :
		m_deviceResources(deviceResources),
		m_width(width),
		m_height(height)
{
#ifdef HOLOLENS
	m_position = { 0.f, 0.f, -2.f };
#endif // HOLOLENS

	CreateDeviceDependentResources();
}

void VideoRenderer::CreateDeviceDependentResources()
{
	VertexPositionTexture vertices[] =
	{
#ifdef HOLOLENS
		// Positions the world-locked hologram two meters in front of the user.

		// Left camera.
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(	1.0f,  1.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 0.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) },

		// Right camera.
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 1.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.5f, 1.0f, 1.0f) },
		{ XMFLOAT3(	1.0f,  1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT3(0.5f, 0.0f, 1.0f) }
#else // HOLOLENS
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(	1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(	1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) }
#endif // HOLOLENS
	};

	D3D11_BUFFER_DESC bufferDesc = { 0 };
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(VertexPositionTexture) * ARRAYSIZE(vertices);
	D3D11_SUBRESOURCE_DATA subresourceData = { vertices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&bufferDesc, &subresourceData, &m_vertexBuffer);

	// Initializes the video texture.
	m_videoTextureDesc = { 0 };
	m_videoTextureDesc.ArraySize = 1;
	m_videoTextureDesc.Usage = D3D11_USAGE_DYNAMIC;
	m_videoTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	m_videoTextureDesc.Width = m_width;
	m_videoTextureDesc.Height = m_height;
	m_videoTextureDesc.MipLevels = 1;
	m_videoTextureDesc.SampleDesc.Count = 1;
	m_videoTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_videoTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	m_deviceResources->GetD3DDevice()->CreateTexture2D(
		&m_videoTextureDesc, nullptr, &m_videoTexture);

	// Creates the shader resource view so that shaders may use it.
	D3D11_SHADER_RESOURCE_VIEW_DESC textureViewDesc;
	ZeroMemory(&textureViewDesc, sizeof(textureViewDesc));
	textureViewDesc.Format = m_videoTextureDesc.Format;
	textureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	textureViewDesc.Texture2D.MipLevels = m_videoTextureDesc.MipLevels;
	textureViewDesc.Texture2D.MostDetailedMip = 0;

	DX::ThrowIfFailed(
		m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
			m_videoTexture,
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

#ifdef HOLOLENS
	auto loadVertexShaderTask = DX::ReadDataAsync(L"VprtVertexShader.cso");
#else // HOLOLENS
	auto loadVertexShaderTask = DX::ReadDataAsync(L"VertexShader.cso");
#endif // HOLOLENS

	loadVertexShaderTask.then([this](std::vector<byte> file)
	{
		m_deviceResources->GetD3DDevice()->CreateVertexShader(
			file.data(), file.size(), nullptr, &m_vertexShader);

		D3D11_INPUT_ELEMENT_DESC elementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
#ifdef HOLOLENS
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
#else // HOLOLENS
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
#endif // HOLOLENS
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
}

void VideoRenderer::ReleaseDeviceDependentResources()
{
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexBuffer.Reset();
}

void VideoRenderer::Update(DX::StepTimer timer)
{
}

void VideoRenderer::UpdateFrame(const uint8_t* data)
{
	m_frameData = data;
}

void VideoRenderer::Render()
{
	// Do not render when the frame data isn't ready.
	if (!m_frameData)
	{
		return;
	}

	// Gets the device context.
	ID3D11DeviceContext* context = m_deviceResources->GetD3DDeviceContext();

#ifndef HOLOLENS
	// Sets the render target view.
	ID3D11RenderTargetView *const targets[1] = 
	{ 
		m_deviceResources->GetBackBufferRenderTargetView() 
	};

	context->OMSetRenderTargets(1, targets, nullptr);
#endif // HOLOLENS

	// Updates the texture data.
	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT result = m_deviceResources->GetD3DDeviceContext()->Map(
		m_videoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (result == S_OK)
	{
		memcpy(mapped.pData, m_frameData, mapped.RowPitch * m_height);
	}

	m_deviceResources->GetD3DDeviceContext()->Unmap(m_videoTexture, 0);

	// Rendering.
	UINT stride = sizeof(VertexPositionTexture);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

#ifdef HOLOLENS
	context->Draw(12, 0);
#else // HOLOLENS
	context->Draw(6, 0);
#endif // HOLOLENS
}
