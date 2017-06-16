#include "pch.h"
#include "CubeRenderer.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace DX;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;
using namespace Toolkit3DSample;

// Eye is at (0, 0.1, 1.0), looking at point (0, 0.1, 0) with the up-vector along the y-axis.
static const XMVECTORF32 eye = { 0.0f, 0.0f, 1.0f, 0.0f };
static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

CubeRenderer::CubeRenderer(DeviceResources* deviceResources) :
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_deviceResources(deviceResources)
{
	InitGraphics();
	InitPipeline();
}

#ifdef STEREO_OUTPUT_MODE
// This function uses a SpatialPointerPose to position the world-locked hologram
// two meters in front of the user's heading.
void CubeRenderer::PositionHologram(SpatialPointerPose^ pointerPose)
{
	if (pointerPose != nullptr)
	{
		// Get the gaze direction relative to the given coordinate system.
		const float3 headPosition = pointerPose->Head->Position;
		const float3 headDirection = pointerPose->Head->ForwardDirection;

		PositionHologram(headPosition, headDirection);
	}
}

// This function uses a point and a vector to position the world-locked hologram
// two meters in front of the user's heading.
void CubeRenderer::PositionHologram(float3 headPosition, float3 headDirection)
{
	// The hologram is positioned two meters along the user's gaze direction.
	static const float distanceFromUser = 2.0f; // meters
	const float3 gazeAtTwoMeters = headPosition + (distanceFromUser * headDirection);

	// This will be used as the translation component of the hologram's
	// model transform.
	SetPosition(gazeAtTwoMeters);
}
#endif // STEREO_OUTPUT_MODE

void CubeRenderer::InitGraphics()
{
	// Load mesh vertices. Each vertex has a position and a color.
	static const VertexPositionColor cubeVertices[] =
	{
		{ XMFLOAT3(-0.1f, -0.1f, -0.1f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // vertex 0 non-debug
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
#ifdef STEREO_OUTPUT_MODE
	errno_t error = fopen_s(&vertexShaderFile, "VertexShaderHololens.cso", "rb");
#else // STEREO_OUTPUT_MODE
	errno_t error = fopen_s(&vertexShaderFile, "VertexShader.cso", "rb");
#endif // STEREO_OUTPUT_MODE
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

#ifdef STEREO_OUTPUT_MODE
	// Creates the geometry shader.
	FILE* geometryShaderFile = nullptr;
	error = fopen_s(&geometryShaderFile, "GeometryShader.cso", "rb");
	fseek(geometryShaderFile, 0, SEEK_END);
	int geometryShaderFileSize = ftell(geometryShaderFile);
	char* geometryShaderFileData = new char[geometryShaderFileSize];
	fseek(geometryShaderFile, 0, SEEK_SET);
	fread(geometryShaderFileData, 1, geometryShaderFileSize, geometryShaderFile);
	fclose(geometryShaderFile);
	m_deviceResources->GetD3DDevice()->CreateGeometryShader(
		geometryShaderFileData,
		geometryShaderFileSize,
		nullptr,
		&m_geometryShader);

	// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
	// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
	// a pass-through geometry shader is used to set the render target 
	// array index.
	m_deviceResources->GetD3DDeviceContext()->GSSetShader(
		m_geometryShader,
		nullptr,
		0
	);
#endif // STEREO_OUTPUT_MODE

	// Cleanup.
	delete []vertexShaderFileData;
	delete []pixelShaderFileData;
#ifdef STEREO_OUTPUT_MODE
	delete []geometryShaderFileData;
#endif // STEREO_OUTPUT_MODE

	// Creates the constant buffer.
#ifdef STEREO_OUTPUT_MODE
	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
#else // STEREO_OUTPUT_MODE
	const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
#endif // STEREO_OUTPUT_MODE
	m_deviceResources->GetD3DDevice()->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer);

#ifndef STEREO_OUTPUT_MODE
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
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
	);

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
#endif // STEREO_OUTPUT_MODE
}

void CubeRenderer::Update(const DX::StepTimer& timer)
{
	// Rotate the cube.
	// Convert degrees to radians, then convert seconds to rotation angle.
	const float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
	const double relativeRotation = timer.GetTotalSeconds() * radiansPerSecond;
	const float radians = static_cast<float>(fmod(relativeRotation, XM_2PI));
	const XMMATRIX modelRotation = XMMatrixRotationY(-radians);

	// Position the cube.
	const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

	// Multiply to get the transform matrix.
	// Note that this transform does not enforce a particular coordinate system. The calling
	// class is responsible for rendering this content in a consistent manner.
	const XMMATRIX modelTransform = XMMatrixMultiply(modelRotation, modelTranslation);

	// Prepares to pass the updated model matrix to the shader.
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(modelTransform));

	// Update the model transform buffer for the hologram.
	m_deviceResources->GetD3DDeviceContext()->UpdateSubresource(
		m_constantBuffer,
		0,
		nullptr,
		&m_constantBufferData,
		0,
		0
	);
}

void CubeRenderer::Render()
{
	// Gets the device context.
	auto context = m_deviceResources->GetD3DDeviceContext();

	// Sets the vertex buffer and index buffer.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Sets the primitive.
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Sends the constant buffer to the graphics device.
	context->VSSetConstantBuffers1(0, 1, &m_constantBuffer, nullptr, nullptr);

#ifdef STEREO_OUTPUT_MODE
	context->DrawIndexedInstanced(
		m_indexCount,       // Index count per instance.
		2,					// Instance count.
		0,                  // Start index location.
		0,                  // Base vertex location.
		0                   // Start instance location.
	);
#else // STEREO_OUTPUT_MODE
	// Sets the render target.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, nullptr);

	// Clear the back buffer.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), Colors::Transparent);

	// Updates view matrix.
	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
	context->UpdateSubresource1(m_constantBuffer, 0, NULL, &m_constantBufferData, 0, 0, 0);

	// Draws the objects.
	context->DrawIndexed(m_indexCount, 0, 0);
#endif // STEREO_OUTPUT_MODE
}
