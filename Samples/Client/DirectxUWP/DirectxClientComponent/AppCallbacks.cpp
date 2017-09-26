#include "pch.h"

#include "AppCallbacks.h"
#include "DirectXHelper.h"

using namespace DirectXClientComponent;
using namespace DirectX;
using namespace Microsoft::WRL::Wrappers;
using namespace Platform;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;

#define VIDEO_FRAME_WIDTH	1280 * 2
#define VIDEO_FRAME_HEIGHT	720

AppCallbacks::AppCallbacks(SendInputDataHandler^ sendInputDataHandler) :
	m_videoRenderer(nullptr),
	m_holographicSpace(nullptr),
	m_sentStereoMode(false),
	m_sendInputDataHandler(sendInputDataHandler)
{
}

AppCallbacks::~AppCallbacks()
{
	delete m_videoRenderer;
}

void AppCallbacks::Initialize(CoreApplicationView^ appView)
{
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	m_main = std::make_unique<HolographicAppMain>(m_deviceResources);
}

void AppCallbacks::SetWindow(CoreWindow^ window)
{
	// Create a holographic space for the core window for the current view.
	// Presenting holographic frames that are created by this holographic
	// space will put the app into exclusive mode.
	m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

	// The DeviceResources class uses the preferred DXGI adapter ID from
	// the holographic space (when available) to create a Direct3D device.
	// The HolographicSpace uses this ID3D11Device to create and manage
	// device-based resources such as swap chains.
	m_deviceResources->SetHolographicSpace(m_holographicSpace);

	// The main class uses the holographic space for updates and rendering.
	m_main->SetHolographicSpace(m_holographicSpace);
}

void AppCallbacks::Run()
{
	while (true)
	{
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
			CoreProcessEventsOption::ProcessAllIfPresent);

		if (m_videoRenderer)
		{
			// Updates.
			HolographicFrame^ holographicFrame = m_main->Update();

			// Renders.
			if (m_main->Render(holographicFrame))
			{
				// The holographic frame has an API that presents the swap chain for each
				// holographic camera.
				m_deviceResources->Present(holographicFrame);

				// Sends view matrix.
				SendInputData(holographicFrame);
			}
		}
	}
}

void AppCallbacks::SetMediaStreamSource(Windows::Media::Core::IMediaStreamSource ^ mediaSourceHandle)
{
	m_mediaSource =
		reinterpret_cast<ABI::Windows::Media::Core::IMediaStreamSource *>(mediaSourceHandle);

	if (m_mediaSource != nullptr)
	{
		// Initializes the media engine player.
		m_player = ref new MEPlayer(m_deviceResources->GetD3DDevice());

		// Creates a dummy renderer and texture until the first frame is received from WebRTC
		ID3D11ShaderResourceView* textureView;
		HRESULT result = m_player->GetPrimaryTexture(
			VIDEO_FRAME_WIDTH, VIDEO_FRAME_HEIGHT, (void**)&textureView);

		// Initializes the video render.
		InitVideoRender(m_deviceResources, textureView);
	}
}

void AppCallbacks::InitVideoRender(
	std::shared_ptr<DX::DeviceResources> deviceResources,
	ID3D11ShaderResourceView* textureView)
{
	// Initializes the video renderer.
	m_videoRenderer = new VideoRenderer(m_deviceResources, textureView);

	// Initializes the new video texture
	m_main->SetVideoRender(m_videoRenderer);
}

void AppCallbacks::SendInputData(HolographicFrame^ holographicFrame)
{
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;
	SpatialCoordinateSystem^ currentCoordinateSystem =
		m_main->GetReferenceFrame()->CoordinateSystem;

	// Attempt to set server to stereo mode
	if (!m_sentStereoMode && m_mediaSource)
	{
		String^ msg =
			"{" +
			"  \"type\":\"stereo-rendering\"," +
			"  \"body\":\"1\"" +
			"}";

		if (m_sendInputDataHandler(msg))
		{
			// The server is now in stereo mode. Start receiving frames.
			// This is required to avoid corrupt frames at startup.
			m_player->SetMediaStreamSource(m_mediaSource);
			m_player->Play();
			m_sentStereoMode = true;
		}
	}

	for (auto cameraPose : prediction->CameraPoses)
	{
		HolographicStereoTransform cameraProjectionTransform =
			cameraPose->ProjectionTransform;

		Platform::IBox<HolographicStereoTransform>^ viewTransformContainer =
			cameraPose->TryGetViewTransform(currentCoordinateSystem);

		if (viewTransformContainer != nullptr)
		{
			HolographicStereoTransform viewCoordinateSystemTransform =
				viewTransformContainer->Value;

			XMFLOAT4X4 leftViewProjectionMatrix;
			XMStoreFloat4x4(
				&leftViewProjectionMatrix,
				XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Left) * XMLoadFloat4x4(&cameraProjectionTransform.Left))
			);

			XMFLOAT4X4 rightViewProjectionMatrix;
			XMStoreFloat4x4(
				&rightViewProjectionMatrix,
				XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Right) * XMLoadFloat4x4(&cameraProjectionTransform.Right))
			);

			// Builds the camera transform message.
			String^ leftCameraTransform = "";
			String^ rightCameraTransform = "";
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					leftCameraTransform += leftViewProjectionMatrix.m[i][j] + ",";
					rightCameraTransform += rightViewProjectionMatrix.m[i][j];
					if (i != 3 || j != 3)
					{
						rightCameraTransform += ",";
					}
				}
			}

			String^ cameraTransformBody = leftCameraTransform + rightCameraTransform;
			String^ msg =
				"{" +
				"  \"type\":\"camera-transform-stereo\"," +
				"  \"body\":\"" + cameraTransformBody + "\"" +
				"}";

			m_sendInputDataHandler(msg);
		}
	}
}

