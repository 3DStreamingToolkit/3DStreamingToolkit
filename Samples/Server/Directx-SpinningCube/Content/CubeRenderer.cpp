#include "pch.h"
#include "CubeRenderer.h"
#include "DirectXHelper.h"
#ifndef TEST_RUNNER
#include "config_parser.h"
#endif // TEST_RUNNER

using namespace DirectX;
using namespace DX;
using namespace StreamingToolkitSample;

#ifndef TEST_RUNNER
using namespace StreamingToolkit;
#endif // TEST_RUNNER

// Eye is at (0, 0, 1), looking at point (0, 0, 0) with the up-vector along the y-axis.
static const XMVECTORF32 eye = { 0.0f, 0.0f, 1.0f, 0.0f };
static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

// Mesh vertices. Each vertex has a position and a color.
static const VertexPositionColor cubeVertices[] =
{
	{ XMFLOAT3(-0.1f, -0.1f, -0.1f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	{ XMFLOAT3(-0.1f, -0.1f,  0.1f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3(-0.1f,  0.1f, -0.1f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	{ XMFLOAT3(-0.1f,  0.1f,  0.1f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
	{ XMFLOAT3( 0.1f, -0.1f, -0.1f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
	{ XMFLOAT3( 0.1f, -0.1f,  0.1f), XMFLOAT3(1.0f, 0.0f, 1.0f) },
	{ XMFLOAT3( 0.1f,  0.1f, -0.1f), XMFLOAT3(1.0f, 1.0f, 0.0f) },
	{ XMFLOAT3( 0.1f,  0.1f,  0.1f), XMFLOAT3(1.0f, 1.0f, 1.0f) },
};

static const unsigned short cubeIndicesLH[] =
{
	2,0,1, // -x
	2,1,3,

	6,5,4, // +x
	6,7,5,

	0,5,1, // -y
	0,4,5,

	2,7,6, // +y
	2,3,7,

	0,6,4, // -z
	0,2,6,

	1,7,3, // +z
	1,5,7
};

static const unsigned short cubeIndicesRH[] =
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
	CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = { cubeVertices , 0, 0 };
	m_deviceResources->GetD3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer);
	m_indexCount = ARRAYSIZE(cubeIndicesLH);
	CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndicesLH), D3D11_BIND_INDEX_BUFFER);
	D3D11_SUBRESOURCE_DATA indexBufferData =
	{
		m_deviceResources->IsStereo() ? cubeIndicesRH : cubeIndicesLH,
		0,
		0
	};

	m_deviceResources->GetD3DDevice()->CreateBuffer(
		&indexBufferDesc, &indexBufferData, &m_indexBuffer);
}

void CubeRenderer::InitPipeline()
{
	// Creates the vertex shader.
	FILE* vertexShaderFile = nullptr;
#ifndef TEST_RUNNER
	errno_t error = fopen_s(
		&vertexShaderFile,
		ConfigParser::GetAbsolutePath("VertexShader.cso").c_str(),
		"rb");
#else // TEST_RUNNER
	errno_t error = fopen_s(&vertexShaderFile, "VertexShader.cso", "rb");
#endif // TEST_RUNNER
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
#ifndef TEST_RUNNER
	error = fopen_s(
		&pixelShaderFile,
		ConfigParser::GetAbsolutePath("PixelShader.cso").c_str(),
		"rb");
#else // TEST_RUNNER
	error = fopen_s(&pixelShaderFile, "PixelShader.cso", "rb");
#endif // TEST_RUNNER

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

	InitConstantBuffers(m_deviceResources->IsStereo());
}

void CubeRenderer::InitConstantBuffers(bool isStereo)
{
	if (isStereo)
	{
		// Initializes the view matrix as identity since we'll use the 
		// projection matrix to store viewProjection transformation.
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

		// This sample makes use of a left-handed coordinate system using 
		// row-major matrices.
		XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovLH(
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
			XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));

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
	// Updates the cube vertice indices.
	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
		m_indexBuffer,
		0,
		NULL,
		m_deviceResources->IsStereo() ? cubeIndicesRH : cubeIndicesLH,
		0,
		0,
		0);

	// Rotates the cube.
	float radians = XMConvertToRadians(m_degreesPerSecond++);
	const XMMATRIX modelRotation = XMMatrixRotationY(radians);

#ifdef TEST_RUNNER
	const XMMATRIX modelTransform = modelRotation;
#else // TEST_RUNNER
	// Positions the cube.
	const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

	// Multiply to get the transform matrix.
	// Note that this transform does not enforce a particular coordinate system. The calling
	// class is responsible for rendering this content in a consistent manner.
	const XMMATRIX modelTransform = XMMatrixMultiply(modelRotation, modelTranslation);
#endif // TEST_RUNNER

	// Prepares to pass the updated model matrix to the shader.
	XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(modelTransform));
	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
		m_modelConstantBuffer, 0, NULL, &m_modelConstantBufferData, 0, 0, 0);
}

void CubeRenderer::Render(ID3D11RenderTargetView* renderTargetView)
{
	// Gets the device context.
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Sets the render target.
	ID3D11RenderTargetView *const targets[1] = { renderTargetView };
	context->OMSetRenderTargets(1, targets, nullptr);

	// Clear the back buffer.
	context->ClearRenderTargetView(renderTargetView, Colors::Black);

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

	// Gets the viewport.
	D3D11_VIEWPORT* viewports = m_deviceResources->GetScreenViewport();

	if (m_deviceResources->IsStereo())
	{
		// Updates view projection matrix for left eye.
		context->UpdateSubresource1(
			m_projectionConstantBuffer, 0, NULL, &m_projectionConstantBufferData[0], 0, 0, 0);

		// Renders the cube.
		context->RSSetViewports(1, viewports);
		context->DrawIndexed(m_indexCount, 0, 0);

		// Updates view projection matrix for right eye.
		context->UpdateSubresource1(
			m_projectionConstantBuffer, 0, NULL, &m_projectionConstantBufferData[1], 0, 0, 0);

		// Renders the cube.
		context->RSSetViewports(1, viewports + 1);
		context->DrawIndexed(m_indexCount, 0, 0);
	}
	else
	{
		// Renders the cube.
		context->RSSetViewports(1, viewports);
		context->DrawIndexed(m_indexCount, 0, 0);
	}
}

void CubeRenderer::Render()
{
	Render(m_deviceResources->GetBackBufferRenderTargetView());
}

void CubeRenderer::UpdateView(const XMFLOAT4X4& viewProjectionLeft, const XMFLOAT4X4& viewProjectionRight)
{
	m_projectionConstantBufferData[0].projection = viewProjectionLeft;
	m_projectionConstantBufferData[1].projection = viewProjectionRight;
}

void CubeRenderer::UpdateView(const XMVECTORF32& eye, const XMVECTORF32& at, const XMVECTORF32& up)
{
	// Updates the view matrix.
	XMStoreFloat4x4(
		&m_viewConstantBufferData.view,
		XMMatrixTranspose(XMMatrixLookAtLH(eye, at, up)));

	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource1(
		m_viewConstantBuffer,
		0,
		NULL,
		&m_viewConstantBufferData,
		0,
		0,
		0);
}
