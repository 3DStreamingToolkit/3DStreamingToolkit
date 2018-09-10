//
// Declaration of the AppCallbacks class.
//

#pragma once

#include "AppCallbacks.g.h"
#include <cstdint>
#include <winrt\Windows.ApplicationModel.Core.h>
#include <winrt\Windows.UI.Core.h>
#include <winrt\Windows.Media.Core.h>

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

	private:
		void SendInputData();
    };
}

namespace winrt::DirectXClientComponent_CppWinRT::factory_implementation
{
    struct AppCallbacks : AppCallbacksT<AppCallbacks, implementation::AppCallbacks>
    {
    };
}
