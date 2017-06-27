#include "pch.h"
#include "CubeRenderer.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace DX;
using namespace Toolkit3DSample;

// Eye is at (0, 0, 1), looking at point (0, 0, 0) with the up-vector along the y-axis.
static XMVECTORF32 eye = { 0.0f, 0.0f, 1.0f, 0.0f };
static XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
static XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

CubeRenderer::CubeRenderer(DeviceResources* deviceResources) :
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_deviceResources(deviceResources)
{
	InitGraphics();
	InitPipeline();
}

void CubeRenderer::InitGraphics()
{
	// Load mesh vertices. Each vertex has a position and a color.
	static const VertexPositionColor cubeVertices[] =
	{
		{ XMFLOAT3(-0.1f, -0.1f, -0.1f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-0.1f, -0.1f,  0.1f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.1f,  0.1f, -0.1f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-0.1f,  0.1f,  0.1f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(0.1f,  -0.1f, -0.1f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(0.1f,  -0.1f,  0.1f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.1f,   0.1f, -0.1f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(0.1f,   0.1f,  0.1f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	};

	CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = { cubeVertices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer);

	// Load mesh indices. Each trio of indices represents
	// a triangle to be rendered on the screen.
	// For example: 0,2,1 means that the vertices with indexes
	// 0, 2 and 1 from the vertex buffer compose the 
	// first triangle of this mesh.
	static const unsigned short cubeIndices[] =
	{
		2,1,0, // -x
		2,3,1,

		6,4,5, // +x
		6,5,7,

		0,1,5, // -y
		0,5,4,

		2,6,7, // +y
		2,7,3,

		0,4,6, // -z
		0,6,2,

		1,3,7, // +z
		1,7,5,
	};

	m_indexCount = ARRAYSIZE(cubeIndices);

	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
	D3D11_SUBRESOURCE_DATA indexBufferData = { cubeIndices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(&indexBufferDesc, &indexBufferData, &m_indexBuffer);
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
	delete []vertexShaderFileData;
	delete []pixelShaderFileData;

	// Creates the model constant buffer.
	CD3D11_BUFFER_DESC modelConstantBufferDesc(
		sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&modelConstantBufferDesc,
		nullptr,
		&m_modelConstantBuffer);

	// Creates the view constant buffer.
	CD3D11_BUFFER_DESC viewConstantBufferDesc(
		sizeof(ViewConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&viewConstantBufferDesc,
		nullptr,
		&m_viewConstantBuffer);

	// Creates the projection constant buffer.
	CD3D11_BUFFER_DESC projectionConstantBufferDesc(
		sizeof(ProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);

	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&projectionConstantBufferDesc,
		nullptr,
		&m_projectionConstantBuffer);

	if (m_deviceResources->IsStereo())
	{
		// Initializes the view matrix as identity since we'll use the projection matrix
		// to store viewProjection transformation.
		XMStoreFloat4x4(&m_viewConstantBufferData.view, XMMatrixIdentity());

		m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
			m_viewConstantBuffer,
			0,
			NULL,
			&m_viewConstantBufferData,
			0,
			0,
			0);
	}
	else
	{
		// Initializes the projection matrix.
		SIZE outputSize = m_deviceResources->GetOutputSize();
		float aspectRatio = (float)outputSize.cx / outputSize.cy;
		float fovAngleY = 70.0f * XM_PI / 180.0f;

		// This is a simple example of change that can be made when the app is in
		// portrait or snapped view.
		if (aspectRatio < 1.0f)
		{
			fovAngleY *= 2.0f;
		}

		// This sample makes use of a right-handed coordinate system using row-major matrices.
		XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
			fovAngleY,
			aspectRatio,
			0.01f,
			100.0f
		);

		// Ignores the orientation.
		XMMATRIX orientationMatrix = XMMatrixIdentity();

		XMStoreFloat4x4(
			&m_projectionConstantBufferData[0].projection,
			XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

		m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
			m_projectionConstantBuffer,
			0,
			NULL,
			&m_projectionConstantBufferData[0],
			0,
			0,
			0);

		// Initializes the view matrix.
		XMStoreFloat4x4(
			&m_viewConstantBufferData.view,
			XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));

		m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
			m_viewConstantBuffer,
			0,
			NULL,
			&m_viewConstantBufferData,
			0,
			0,
			0);
	}
}

void CubeRenderer::Update()
{
	// Converts to radians.
	float radians = XMConvertToRadians(m_degreesPerSecond++);

	// Prepares to pass the updated model matrix to the shader.
	XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
		m_modelConstantBuffer, 0, NULL, &m_modelConstantBufferData, 0, 0, 0);
}

void CubeRenderer::Render()
{
	// Gets the device context.
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Sets the render target.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, nullptr);

	// Clear the back buffer.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), Colors::Black);

	// Sets the vertex buffer and index buffer.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Sets the primitive.
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Sends the model constant buffer to the graphics device.
	context->VSSetConstantBuffers(0, 1, &m_modelConstantBuffer);

	// Sends the view constant buffer to the graphics device.
	context->VSSetConstantBuffers(1, 1, &m_viewConstantBuffer);

	// Sends the projection constant buffer to the graphics device.
	context->VSSetConstantBuffers(2, 1, &m_projectionConstantBuffer);

	if (m_deviceResources->IsStereo())
	{
		// Gets the viewport.
		D3D11_VIEWPORT* viewports = m_deviceResources->GetScreenViewport();

		// Updates view projection matrix for the left eye.
		context->UpdateSubresource1(
			m_projectionConstantBuffer, 0, NULL, &m_projectionConstantBufferData[0], 0, 0, 0);

		// Draws the objects in the left eye.
		context->RSSetViewports(1, viewports);
		context->DrawIndexed(m_indexCount, 0, 0);

		// Updates view projection matrix for the right eye.
		context->UpdateSubresource1(
			m_projectionConstantBuffer, 0, NULL, &m_projectionConstantBufferData[1], 0, 0, 0);

		// Draws the objects in the right eye.
		context->RSSetViewports(1, viewports + 1);
		context->DrawIndexed(m_indexCount, 0, 0);
	}
	else
	{
		// Draws the objects.
		context->DrawIndexed(m_indexCount, 0, 0);
	}
}

void CubeRenderer::UpdateView(const XMFLOAT4X4& viewProjectionLeft, const XMFLOAT4X4& viewProjectionRight)
{
	m_projectionConstantBufferData[0].projection = viewProjectionLeft;
	m_projectionConstantBufferData[1].projection = viewProjectionRight;
}

void CubeRenderer::UpdateView(const XMVECTORF32& newEye, const XMVECTORF32& newLookAt, const XMVECTORF32& newUp)
{
	eye = newEye;
	at = newLookAt;
	up = newUp;

	// Updates the view matrix.
	XMStoreFloat4x4(
		&m_viewConstantBufferData.view,
		XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));

	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
		m_viewConstantBuffer,
		0,
		NULL,
		&m_viewConstantBufferData,
		0,
		0,
		0);
}
