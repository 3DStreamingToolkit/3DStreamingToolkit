#include "pch.h"

#include "AppCallbacks.h"
#include "DirectXHelper.h"

using namespace DirectXClientComponent;
using namespace DirectX;
using namespace Microsoft::WRL::Wrappers;
using namespace Platform;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception;
using namespace Windows::Perception::Spatial;
using namespace Windows::System::Threading;

#define VIDEO_FRAME_WIDTH	1280
#define VIDEO_FRAME_HEIGHT	720
#define INPUT_RATE			1000 / 30

#define MAX_FRAMES 10000
HolographicFrame^ holographicFrames[MAX_FRAMES];
int currentFrameId = 0;

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
	// Starts the timer to send input data.
	TimeSpan period;
	period.Duration = INPUT_RATE * 10000;
	ThreadPoolTimer^ PeriodicTimer = ThreadPoolTimer::CreatePeriodicTimer(
		ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
		{
			if (m_videoRenderer)
			{
				auto lock = m_lock.Lock();
				SendInputData();
			}
		}),
		period);

	// Starts the main loop.
	while (true)
	{
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
			CoreProcessEventsOption::ProcessOneAndAllPending);
	}
}

void AppCallbacks::SetMediaStreamSource(Windows::Media::Core::IMediaStreamSource ^ mediaSourceHandle)
{
	auto lock = m_lock.Lock();
	m_mediaSource = reinterpret_cast<ABI::Windows::Media::Core::IMediaStreamSource *>(mediaSourceHandle);

	if (m_mediaSource != nullptr)
	{
		// Initializes the media engine player.
		m_player = ref new MEPlayer(m_deviceResources->GetD3DDevice());

		// Creates a dummy renderer and texture until the first frame is received from WebRTC
		ID3D11ShaderResourceView* textureView;
		HRESULT result = m_player->GetPrimaryTexture(
			VIDEO_FRAME_WIDTH, VIDEO_FRAME_HEIGHT, (void**)&textureView);

		// Initializes the video render.
		m_videoRenderer = new VideoRenderer(m_deviceResources, VIDEO_FRAME_WIDTH, VIDEO_FRAME_HEIGHT);
		m_main->SetVideoRender(m_videoRenderer);

		// Sets the frame transfer callback.
		m_player->FrameTransferred += ref new MEPlayer::VideoFrameTransferred(
			[&](MEPlayer^ mc, int width, int height, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, LONGLONG pts)
		{
			auto lock = m_lock.Lock();
			if (!m_framePredictionTimestamp.empty())
			{
				int frameId = (pts % 10000) + 1;
				int64_t predictionTimestamp = m_framePredictionTimestamp.at(frameId);
				//OutputDebugString((L"FrameTransferred: " + predictionTimestamp + L"\r\n")->Data());

				for (int i = 0; i < MAX_FRAMES; i++)
				{
					if (!holographicFrames[i])
					{
						continue;
					}

					PerceptionTimestamp^ perceptionTimestamp = holographicFrames[i]->CurrentPrediction->Timestamp;
					if (predictionTimestamp == perceptionTimestamp->TargetTime.UniversalTime)
					{
						m_deviceResources->GetD3DDeviceContext()->CopyResource(
							m_videoRenderer->GetVideoFrame(), texture.Get());

						if (m_main->Render(holographicFrames[i]))
						{
							// The holographic frame has an API that presents the swap chain for each
							// holographic camera.
							m_deviceResources->Present(holographicFrames[i]);
							holographicFrames[i] = nullptr;
						}

						break;
					}
				}
			}
		});
	}
}

void AppCallbacks::OnSampleTimestamp(int64_t timestamp)
{
	auto lock = m_lock.Lock();
	m_framePredictionTimestamp.push_back(timestamp);
}

void AppCallbacks::SendInputData()
{
	// Attempts to set server to stereo mode.
	if (!m_sentStereoMode)
	{
		String^ msg =
			"{" +
			"  \"type\":\"stereo-rendering\"," +
			"  \"body\":\"1\"" +
			"}";

		if (m_mediaSource && m_sendInputDataHandler(msg))
		{
			// The server is now in stereo mode. Start receiving frames.
			// This is required to avoid corrupt frames at startup.
			m_player->SetMediaStreamSource(m_mediaSource.Get());
			m_player->Play();
			m_sentStereoMode = true;
		}
		else
		{
			return;
		}
	}

	HolographicFrame^ newFrame = m_main->Update();
	if (currentFrameId == MAX_FRAMES)
	{
		currentFrameId = 0;
	}

	holographicFrames[currentFrameId++] = newFrame;
	SpatialCoordinateSystem^ currentCoordinateSystem = m_main->GetReferenceFrame()->CoordinateSystem;
	XMFLOAT4X4 leftViewMatrix;
	XMFLOAT4X4 rightViewMatrix;
	for (auto cameraPose : newFrame->CurrentPrediction->CameraPoses)
	{
		HolographicStereoTransform cameraProjectionTransform =
			cameraPose->ProjectionTransform;

		Platform::IBox<HolographicStereoTransform>^ viewTransformContainer =
			cameraPose->TryGetViewTransform(currentCoordinateSystem);

		if (viewTransformContainer != nullptr)
		{
			HolographicStereoTransform viewCoordinateSystemTransform =
				viewTransformContainer->Value;

			XMStoreFloat4x4(
				&leftViewMatrix,
				XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Left) * XMLoadFloat4x4(&cameraProjectionTransform.Left))
			);

			XMStoreFloat4x4(
				&rightViewMatrix,
				XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Right) * XMLoadFloat4x4(&cameraProjectionTransform.Right))
			);
		}
	}

	// Builds the camera transform message.
	String^ leftCameraTransform = "";
	String^ rightCameraTransform = "";
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			leftCameraTransform += leftViewMatrix.m[i][j] + ",";
			rightCameraTransform += rightViewMatrix.m[i][j] + ",";
		}
	}

	String^ cameraTransformBody = leftCameraTransform + rightCameraTransform;

	// Adds the current prediction timestamp.
	cameraTransformBody += newFrame->CurrentPrediction->Timestamp->TargetTime.UniversalTime;
	String^ msg =
		"{" +
		"  \"type\":\"camera-transform-stereo\"," +
		"  \"body\":\"" + cameraTransformBody + "\"" +
		"}";

	m_sendInputDataHandler(msg);
}

