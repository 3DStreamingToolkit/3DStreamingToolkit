#include "pch.h"
#include "AppCallbacks.h"

using namespace winrt;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;
using namespace Windows::Media::Core;

namespace winrt::DirectXClientComponent_CppWinRT::implementation
{
	AppCallbacks::AppCallbacks(SendInputDataHandler const& sendInputDataHandler)
	{
	}

	void AppCallbacks::Initialize(CoreApplicationView const& appView)
	{
	}

	void AppCallbacks::SetWindow(CoreWindow const& window)
	{
	}

	void AppCallbacks::SetMediaStreamSource(IMediaStreamSource const& mediaSourceHandle, int32_t width, int32_t height)
	{
	}

	void AppCallbacks::Run()
	{

	}

	void AppCallbacks::OnPredictionTimestamp(int32_t id, int64_t timestamp)
	{
	}

	uint32_t AppCallbacks::FpsReport()
	{
		return 0;
	}

	void AppCallbacks::SendInputData()
	{

	}
}
