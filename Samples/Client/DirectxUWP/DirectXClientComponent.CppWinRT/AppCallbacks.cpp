#include "pch.h"
#include "AppCallbacks.h"
#include <DirectXMath.h>
#include <sstream>

using namespace ::DirectXClientComponent_CppWinRT;
using namespace DirectX;
using namespace winrt;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Perception::Spatial;

#ifdef SHOW_DEBUG_INFO
int64_t g_totalDelayTime = 0;
int64_t g_currentTimestamp = 0;
int g_latencyCounter = 0;
int g_latency = 0;
#endif // SHOW_DEBUG_INFO

namespace winrt::DirectXClientComponent_CppWinRT::implementation
{
	AppCallbacks::AppCallbacks(SendInputDataHandler const& sendInputDataHandler) :
		m_videoRenderer(nullptr),
		m_holographicSpace(nullptr),
		m_sentStereoMode(false),
		m_sendInputDataHandler(sendInputDataHandler)
	{
	}

	void AppCallbacks::Close()
	{
		delete m_videoRenderer;
	}

	void AppCallbacks::Initialize(CoreApplicationView const& appView)
	{
		m_deviceResources = std::make_shared<DX::DeviceResources>();
		m_main = std::make_unique<HolographicAppMain>(m_deviceResources);
	}

	void AppCallbacks::SetWindow(CoreWindow const& window)
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
			CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
				CoreProcessEventsOption::ProcessAllIfPresent);

			SendInputData();
			if (m_player)
			{
				m_player->OnVSyncTimer();
			}
		}
	}

	void AppCallbacks::SetMediaStreamSource(IMediaStreamSource const& mediaSourceHandle, int32_t width, int32_t height)
	{
		auto lock = m_lock.Lock();
		m_mediaSource = mediaSourceHandle.as<ABI::Windows::Media::Core::IMediaStreamSource>();

		if (m_mediaSource != nullptr)
		{
			// Initializes the media engine player.
			m_player = winrt::make_self<CppMEPlayer>(m_deviceResources->GetD3DDevice(), false);

			// Creates a dummy renderer and texture until the first frame is received from WebRTC
			ID3D11ShaderResourceView* textureView;
			HRESULT result = m_player->GetPrimaryTexture(
				width, height, (void**)&textureView);

			// Initializes the video render.
			m_videoRenderer = new VideoRenderer(m_deviceResources, width, height);
			m_main->SetVideoRender(m_videoRenderer);

			// Sets the frame transfer callback.
			m_player->FrameTransferred.add(
				[&](CppMEPlayer* const mc, int width, int height, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, int timestampId)
			{
				auto lock = m_lock.Lock();
				if (!m_framePredictionTimestamp.empty())
				{
					int64_t predictionTimestamp = m_framePredictionTimestamp[timestampId];
					std::vector<HolographicFrame>::iterator it = std::find_if(
						m_holographicFrames.begin(),
						m_holographicFrames.end(),
						[&](HolographicFrame const& frame)
					{
						//see remarks here for time conversion from cx to cppwinrt: https://docs.microsoft.com/en-us/uwp/api/Windows.Foundation.DateTime
						return frame.CurrentPrediction().Timestamp().TargetTime().time_since_epoch().count() ==
							predictionTimestamp;
					});

					if (it != m_holographicFrames.end())
					{
						m_deviceResources->GetD3DDeviceContext()->CopyResource(
							m_videoRenderer->GetVideoFrame(), texture.Get());

						HolographicFrame frame = *it;

#ifdef SHOW_DEBUG_INFO
						if (++g_latencyCounter % 60)
						{
							g_totalDelayTime += (g_currentTimestamp - predictionTimestamp) / 10000;
						}
						else
						{
							g_latency = g_totalDelayTime / 60;
							g_totalDelayTime = 0;
						}

						if (m_main->Render(frame, m_player->GetFrameRate(), g_latency))
#else // SHOW_DEBUG_INFO
						if (m_main->Render(frame))
#endif // SHOW_DEBUG_INFO
						{
							m_deviceResources->Present(frame);
						}

						m_holographicFrames.erase(it);
					}
				}
			});

			m_player->SetMediaStreamSource(m_mediaSource.get());
			m_player->Play();
		}
	}

	void AppCallbacks::OnPredictionTimestamp(int32_t id, int64_t timestamp)
	{
		auto lock = m_lock.Lock();
		m_framePredictionTimestamp[id] = timestamp;
	}

	uint32_t AppCallbacks::FpsReport()
	{
		return m_player->GetFrameRate();
	}

	void AppCallbacks::SendInputData()
	{
		// Attempts to set server to stereo mode.
		if (!m_sentStereoMode)
		{
			std::wstring msg =
				L"{\n"
				L"  \"type\":\"stereo-rendering\",\n"
				L"  \"body\":\"1\"\n"
				L"}\n";

			if (!m_sendInputDataHandler(msg))
			{
				return;
			}

			m_sentStereoMode = true;
		}

		// Creates a new frame for input data.
		HolographicFrame newFrame = m_main->Update();
		m_holographicFrames.push_back(newFrame);

		// Gets the current camera transformation.
		XMFLOAT4X4 leftProjectionMatrix;
		XMFLOAT4X4 leftViewMatrix;
		XMFLOAT4X4 rightProjectionMatrix;
		XMFLOAT4X4 rightViewMatrix;
		SpatialCoordinateSystem currentCoordinateSystem = m_main->GetReferenceFrame().CoordinateSystem();
		for (auto cameraPose : newFrame.CurrentPrediction().CameraPoses())
		{
			HolographicStereoTransform cameraProjectionTransform =
				cameraPose.ProjectionTransform();

			auto viewTransformContainer =
				cameraPose.TryGetViewTransform(currentCoordinateSystem);

			if (viewTransformContainer != nullptr)
			{
				HolographicStereoTransform viewCoordinateSystemTransform =
					viewTransformContainer.Value();

				XMStoreFloat4x4(
					&leftProjectionMatrix,
					XMMatrixTranspose(XMLoadFloat4x4(&cameraProjectionTransform.Left))
				);

				XMStoreFloat4x4(
					&leftViewMatrix,
					XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Left))
				);

				XMStoreFloat4x4(
					&rightProjectionMatrix,
					XMMatrixTranspose(XMLoadFloat4x4(&cameraProjectionTransform.Right))
				);

				XMStoreFloat4x4(
					&rightViewMatrix,
					XMMatrixTranspose(XMLoadFloat4x4(&viewCoordinateSystemTransform.Right))
				);
			}
		}

		// Builds the camera transform message to send.
		std::wstringstream leftProjection;
		std::wstringstream leftView;
		std::wstringstream rightProjection;
		std::wstringstream rightView;

		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				leftProjection << leftProjectionMatrix.m[i][j] << L",";
				leftView << leftViewMatrix.m[i][j] << L",";
				rightProjection << rightProjectionMatrix.m[i][j] << L",";
				rightView << rightViewMatrix.m[i][j] << L",";
			}
		}

		std::wstringstream cameraTransformBody;
		cameraTransformBody << leftProjection.str() << leftView.str() << rightProjection.str() << rightView.str();

		// Adds the current prediction timestamp.
		cameraTransformBody << newFrame.CurrentPrediction().Timestamp().TargetTime().time_since_epoch().count();
		std::wstringstream msg;
			msg << L"{" <<
			L"  \"type\":\"camera-transform-stereo-prediction\"," <<
			L"  \"body\":\"" << cameraTransformBody.str() << L"\"" <<
			"}";

#ifdef SHOW_DEBUG_INFO
		g_currentTimestamp = newFrame.CurrentPrediction().Timestamp().TargetTime().time_since_epoch().count();
#endif // SHOW_DEBUG_INFO

		m_sendInputDataHandler(msg.str());
	}
}
