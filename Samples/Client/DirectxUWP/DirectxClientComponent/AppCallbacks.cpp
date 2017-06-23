#include "pch.h"
#include "AppCallbacks.h"
#include "libyuv/convert.h"


using namespace DirectXClientComponent;
using namespace Platform;
using namespace Windows::System::Profile;
#ifdef HOLOLENS
using namespace Windows::Graphics::Holographic;
#endif // HOLOLENS

static uint8_t* s_videoYUVFrame = nullptr;
static uint8_t* s_videoRGBFrame = nullptr;
static uint8_t* s_videoDataY = nullptr;
static uint8_t* s_videoDataU = nullptr;
static uint8_t* s_videoDataV = nullptr;

AppCallbacks::AppCallbacks() :
	m_videoRenderer(nullptr),
#ifndef HOLOLENS
	m_videoDecoder(nullptr)
#else // HOLOLENS
	m_holographicSpace(nullptr)
#endif // HOLOLENS
{
	AnalyticsVersionInfo^ deviceInfo = AnalyticsInfo::VersionInfo;
	if (deviceInfo->DeviceFamily == "Windows.Holographic")
	{
		
	}
		
}

AppCallbacks::~AppCallbacks()
{
	delete m_videoRenderer;
	delete []s_videoYUVFrame;
	delete []s_videoRGBFrame;
	delete []s_videoDataY;
	delete []s_videoDataU;
	delete []s_videoDataV;
}

void AppCallbacks::Initialize(CoreApplicationView^ appView)
{
	m_deviceResources = std::make_shared<DX::DeviceResources>();

#ifdef HOLOLENS
	m_main = std::make_unique<HolographicAppMain>(m_deviceResources);
#endif // HOLOLENS
}

void AppCallbacks::SetWindow(CoreWindow^ window)
{
#ifdef HOLOLENS
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
#else // HOLOLENS
	m_deviceResources->SetWindow(window);
#endif // HOLOLENS
}

void AppCallbacks::Run()
{
	while (true)
	{
		CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(
			CoreProcessEventsOption::ProcessAllIfPresent);
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
#ifdef HOLOLENS
		m_main->SetVideoRender(m_videoRenderer);
#endif // HOLOLENS
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

#ifdef HOLOLENS
	HolographicFrame^ holographicFrame = m_main->Update();
	if (m_main->Render(holographicFrame))
	{
		// The holographic frame has an API that presents the swap chain for each
		// holographic camera.
		m_deviceResources->Present(holographicFrame);
	}
#else // HOLOLENS
	m_videoRenderer->Render();
	m_deviceResources->Present();
#endif // HOLOLENS
}

void AppCallbacks::OnDecodedFrame(
	uint32_t width,
	uint32_t height,
	const Array<uint8_t>^ encodedData)
{
	// Uses lazy initialization.
	if (!m_videoRenderer)
	{
		// Initializes the video renderer.
		m_videoRenderer = new VideoRenderer(m_deviceResources, width, height);
#ifdef HOLOLENS
		m_main->SetVideoRender(m_videoRenderer);
#endif // HOLOLENS

		// Initalizes the temp buffers.
		int bufferSize = width * height * 4;
		int half_width = (width + 1) / 2;
		size_t size_y = static_cast<size_t>(width) * height;
		size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);

		s_videoRGBFrame = new uint8_t[bufferSize];
		memset(s_videoRGBFrame, 0, bufferSize);

		s_videoYUVFrame = new uint8_t[bufferSize];
		memset(s_videoYUVFrame, 0, bufferSize);

		s_videoDataY = new uint8_t[size_y];
		memset(s_videoDataY, 0, size_y);

		s_videoDataU = new uint8_t[size_uv];
		memset(s_videoDataY, 0, size_uv);

		s_videoDataV = new uint8_t[size_uv];
		memset(s_videoDataY, 0, size_uv);
	}

	if (!libyuv::YUY2ToARGB(
		encodedData->Data,
		width * 2,
		s_videoRGBFrame,
		width * 4,
		width,
		height))
	{
		m_videoRenderer->UpdateFrame(s_videoRGBFrame);

#ifdef HOLOLENS
		HolographicFrame^ holographicFrame = m_main->Update();
		if (m_main->Render(holographicFrame))
		{
			// The holographic frame has an API that presents the swap chain for each
			// holographic camera.
			m_deviceResources->Present(holographicFrame);
		}
#else // HOLOLENS
		m_videoRenderer->Render();
		m_deviceResources->Present();
#endif // HOLOLENS
	}
}

void AppCallbacks::ReadI420Buffer(
	int width,
	int height,
	uint8* buffer,
	uint8** dataY,
	int* strideY,
	uint8** dataU,
	int* strideU,
	uint8** dataV,
	int* strideV)
{
	int half_width = (width + 1) / 2;
	size_t size_y = static_cast<size_t>(width) * height;
	size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);
	*strideY = width;
	*strideU = half_width;
	*strideV = half_width;
	memcpy(*dataY, buffer, size_y);
	memcpy(*dataU, buffer + *strideY * height, size_uv);
	memcpy(*dataV, buffer + *strideY * height + *strideU * ((height + 1) / 2), size_uv);
}
