#pragma once

#include "pch.h"
#include "DeviceResources.h"
#include "VideoRenderer.h"
#include "HolographicAppMain.h"
#include "MediaEnginePlayer.h"

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Media::Core;
using namespace Windows::UI::Core;

namespace DirectXClientComponent
{
	public delegate bool SendInputDataHandler(String^ msg);

	public ref class AppCallbacks sealed
	{
	public:
		AppCallbacks(SendInputDataHandler^ sendInputDataHandler);
		virtual ~AppCallbacks();

		void Initialize(CoreApplicationView^ appView);

		void SetWindow(CoreWindow^ window);

		void Run();

		void SetMediaStreamSource(IMediaStreamSource^ mediaSourceHandle);

		void OnSampleTimestamp(int id, int64_t timestamp);

		uint32 OnFpsReportRequested();

	private:
		void SendInputData();

		Microsoft::WRL::Wrappers::CriticalSection				m_lock;
		std::shared_ptr<DX::DeviceResources>					m_deviceResources;
		VideoRenderer*											m_videoRenderer;
		MEPlayer^												m_player;
		std::unique_ptr<HolographicAppMain>						m_main;
		SendInputDataHandler^									m_sendInputDataHandler;
		bool													m_sentStereoMode;
		ComPtr<ABI::Windows::Media::Core::IMediaStreamSource>	m_mediaSource;
		
		// Frame prediction
		std::vector<HolographicFrame^>							m_holographicFrames;
		std::map<int, int64_t>									m_framePredictionTimestamp;

		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace^		m_holographicSpace;
	};
}