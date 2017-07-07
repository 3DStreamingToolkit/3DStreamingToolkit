#include "pch.h"

#include "AppCallbacks.h"
#include "DirectXHelper.h"
#include "libyuv/convert.h"

using namespace DirectXClientComponent;
using namespace DirectX;
using namespace Microsoft::WRL::Wrappers;
using namespace Platform;
using namespace Windows::System::Profile;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;

static uint8_t* s_videoRGBFrame = nullptr;
static CriticalSection s_lock;

AppCallbacks::AppCallbacks(SendInputDataHandler^ sendInputDataHandler) :
	m_videoRenderer(nullptr),
	m_holographicSpace(nullptr),
	m_sendInputDataHandler(sendInputDataHandler)
{
}

AppCallbacks::~AppCallbacks()
{
	delete m_videoRenderer;
	delete []s_videoRGBFrame;
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
			auto lock = s_lock.Lock();

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

void AppCallbacks::OnFrame(
	uint32_t width,
	uint32_t height,
	const Array<uint8_t>^ dataY,
	uint32_t strideY,
	const Array<uint8_t>^ dataU,
	uint32_t strideU,
	const Array<uint8_t>^ dataV,
	uint32_t strideV)
{
	if (!s_videoRGBFrame)
	{
		m_videoRenderer = new VideoRenderer(m_deviceResources, width, height);
		s_videoRGBFrame = new uint8_t[width * height * 4];
		m_main->SetVideoRender(m_videoRenderer);
	}

	libyuv::I420ToARGB(
		dataY->Data,
		strideY,
		dataU->Data,
		strideU,
		dataV->Data,
		strideV,
		s_videoRGBFrame,
		width * 4,
		width,
		height);

	m_videoRenderer->UpdateFrame(s_videoRGBFrame);
}

void AppCallbacks::OnDecodedFrame(
	uint32_t width,
	uint32_t height,
	const Array<uint8_t>^ decodedData)
{
	auto lock = s_lock.Lock();

	if (!m_videoRenderer)
	{
		// Enables the stereo output mode.
		String^ msg =
			"{" +
			"  \"type\":\"stereo-rendering\"," +
			"  \"body\":\"1\"" +
			"}";

		m_sendInputDataHandler(msg);
	}

	InitVideoRender(m_deviceResources, width, height);

	if (!libyuv::YUY2ToARGB(
		decodedData->Data,
		width * 2,
		s_videoRGBFrame,
		width * 4,
		width,
		height))
	{
		m_videoRenderer->UpdateFrame(s_videoRGBFrame);
	}
}

void AppCallbacks::InitVideoRender(
	std::shared_ptr<DX::DeviceResources> deviceResources,
	int width,
	int height)
{
	if (m_videoRenderer)
	{
		// Do nothing if width and height don't change.
		if (m_videoRenderer->GetWidth() == width &&
			m_videoRenderer->GetHeight() == height)
		{
			return;
		}

		// Releases old resources.
		delete m_videoRenderer;
		delete []s_videoRGBFrame;
	}

	// Initializes the video renderer.
	m_videoRenderer = new VideoRenderer(m_deviceResources, width, height);
	m_main->SetVideoRender(m_videoRenderer);

	// Initalizes the temp buffers.
	int bufferSize = width * height * 4;
	int half_width = (width + 1) / 2;
	size_t size_y = static_cast<size_t>(width) * height;
	size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);

	s_videoRGBFrame = new uint8_t[bufferSize];
	memset(s_videoRGBFrame, 0, bufferSize);
}

void AppCallbacks::SendInputData(HolographicFrame^ holographicFrame)
{
	HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;
	SpatialCoordinateSystem^ currentCoordinateSystem =
		m_main->GetReferenceFrame()->CoordinateSystem;

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
