#include "pch.h"
#include "CubeRenderer.h"
#include "DirectXHelper.h"

using namespace DX;
using namespace Toolkit3DSample;

CubeRenderer::CubeRenderer(DeviceResources* deviceResources) :
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
	// Creates the vertex shader.
	FILE* vertexShaderFile = nullptr;
	errno_t error = fopen_s(&vertexShaderFile, "VertexShader.cso", "rb");
	fseek(vertexShaderFile, 0, SEEK_END);
	int vertexShaderFileSize = ftell(vertexShaderFile);
	char* vertexShaderFileData = new char[vertexShaderFileSize];
	fseek(vertexShaderFile, 0, SEEK_SET);
	fread(vertexShaderFileData, 1, vertexShaderFileSize, vertexShaderFile);
	fclose(vertexShaderFile);
	m_deviceResources->GetD3DDevice()->CreateVertexShader(
		vertexShaderFileData,
		vertexShaderFileSize,
		nullptr, 
		&m_vertexShader);

	// Creates the input layout.
	D3D11_INPUT_ELEMENT_DESC elementDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	m_deviceResources->GetD3DDevice()->CreateInputLayout(
		elementDesc,
		ARRAYSIZE(elementDesc),
		vertexShaderFileData,
		vertexShaderFileSize,
		&m_inputLayout);

	// Sets the input layout and vertex shader.
	m_deviceResources->GetD3DDeviceContext()->IASetInputLayout(m_inputLayout);
	m_deviceResources->GetD3DDeviceContext()->VSSetShader(m_vertexShader, nullptr, 0);

	// Creates the pixel shader.
	FILE* pixelShaderFile = nullptr;
	error = fopen_s(&pixelShaderFile, "PixelShader.cso", "rb");
	fseek(pixelShaderFile, 0, SEEK_END);
	int pixelShaderFileSize = ftell(pixelShaderFile);
	char* pixelShaderFileData = new char[pixelShaderFileSize];
	fseek(pixelShaderFile, 0, SEEK_SET);
	fread(pixelShaderFileData, 1, pixelShaderFileSize, pixelShaderFile);
	fclose(pixelShaderFile);
	m_deviceResources->GetD3DDevice()->CreatePixelShader(
		pixelShaderFileData,
		pixelShaderFileSize,
		nullptr,
		&m_pixelShader);
	
	// Sets the pixel shader.
	m_deviceResources->GetD3DDeviceContext()->PSSetShader(m_pixelShader, nullptr, 0);

	// Cleanup.
	delete[]vertexShaderFileData;
	delete[]pixelShaderFileData;
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
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->Draw(3, 0);
}
