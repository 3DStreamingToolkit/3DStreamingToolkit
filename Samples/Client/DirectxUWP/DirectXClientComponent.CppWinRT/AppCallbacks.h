//
// Declaration of the AppCallbacks class.
//

#pragma once

#include "AppCallbacks.g.h"
#include "Common\DeviceResources.h"
#include "Content\VideoRenderer.h"
#include "HolographicAppMain.h"
#include "MediaEngine\CppMediaEnginePlayer.h"
#include <cstdint>
#include <winrt\Windows.ApplicationModel.Core.h>
#include <winrt\Windows.UI.Core.h>
#include <winrt\Windows.Media.Core.h>
#include <wrl\wrappers\corewrappers.h>

namespace winrt::DirectXClientComponent_CppWinRT::implementation
{
    struct AppCallbacks : AppCallbacksT<AppCallbacks>
    {
		AppCallbacks(SendInputDataHandler const& sendInputDataHandler);
		void Initialize(winrt::Windows::ApplicationModel::Core::CoreApplicationView const& appView);
		void SetWindow(winrt::Windows::UI::Core::CoreWindow const& window);
		void Run();
		void SetMediaStreamSource(winrt::Windows::Media::Core::IMediaStreamSource const& mediaSourceHandle, int32_t width, int32_t height);
		void OnPredictionTimestamp(int32_t id, int64_t timestamp);
		uint32_t FpsReport();
		void Close();

	private:
		void SendInputData();

		Microsoft::WRL::Wrappers::CriticalSection										m_lock;
		std::shared_ptr<DX::DeviceResources>											m_deviceResources;
		::DirectXClientComponent_CppWinRT::VideoRenderer*								m_videoRenderer;
		winrt::com_ptr<CppMEPlayer>														m_player;
		std::unique_ptr<::DirectXClientComponent_CppWinRT::HolographicAppMain>			m_main;
		SendInputDataHandler															m_sendInputDataHandler;
		bool																			m_sentStereoMode;
		winrt::com_ptr<ABI::Windows::Media::Core::IMediaStreamSource>					m_mediaSource;

		// Frame prediction
		std::vector<winrt::Windows::Graphics::Holographic::HolographicFrame>			m_holographicFrames;
		std::map<int, int64_t>															m_framePredictionTimestamp;

		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace								m_holographicSpace = nullptr;
    };
}

namespace winrt::DirectXClientComponent_CppWinRT::factory_implementation
{
    struct AppCallbacks : AppCallbacksT<AppCallbacks, implementation::AppCallbacks>
    {
    };
}
