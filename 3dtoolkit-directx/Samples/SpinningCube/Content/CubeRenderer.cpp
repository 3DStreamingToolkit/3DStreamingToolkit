#include "pch.h"
#include "CubeRenderer.h"
#include "DirectXHelper.h"

using namespace Windows::UI::Core;
using namespace Toolkit3DSample;

CubeRenderer::CubeRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) : 
	m_deviceResources(deviceResources)
{
	InitGraphics();
	InitPipeline();
}

void CubeRenderer::InitGraphics()
{
	VERTEX vertices[] =
	{
		{ 0.0f, 0.5f, 0.0f },
		{ 0.45f, -0.5f, 0.0f },
		{ -0.45f, -0.5f, 0.0f },
	};

	D3D11_BUFFER_DESC bufferDesc = { 0 };
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(VERTEX) * ARRAYSIZE(vertices);
	D3D11_SUBRESOURCE_DATA subresourceData = { vertices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(&bufferDesc, &subresourceData, &m_vertexBuffer);
}

void CubeRenderer::InitPipeline()
{
	auto loadVertexShaderTask = DX::ReadDataAsync(L"VertexShader.cso");
	loadVertexShaderTask.then([this](std::vector<byte> file)
	{
		m_deviceResources->GetD3DDevice()->CreateVertexShader(file.data(), file.size(), nullptr, &m_vertexShader);
		D3D11_INPUT_ELEMENT_DESC elementDesc[] = 
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		m_deviceResources->GetD3DDevice()->CreateInputLayout(
			elementDesc,
			ARRAYSIZE(elementDesc),
			file.data(),
			file.size(),
			&m_inputLayout);

		m_deviceResources->GetD3DDeviceContext()->IASetInputLayout(m_inputLayout.Get());
		m_deviceResources->GetD3DDeviceContext()->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	});

	auto loadPixelShaderTask = DX::ReadDataAsync(L"PixelShader.cso");
	loadPixelShaderTask.then([this](std::vector<byte> file)
	{
		m_deviceResources->GetD3DDevice()->CreatePixelShader(file.data(), file.size(), nullptr, &m_pixelShader);
		m_deviceResources->GetD3DDeviceContext()->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	});
}

void CubeRenderer::Update()
{
}

void CubeRenderer::Render()
{
	auto context = m_deviceResources->GetD3DDeviceContext();

	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, nullptr);

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->Draw(3, 0);
}
