#pragma once

#include <d3d11.h>

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Microsoft::WRL;

namespace Toolkit3DLibrary
{
    ref class VideoHelper sealed
    {
	internal:
		VideoHelper()
		{

		}

		void Initialize(IDXGISwapChain* swapChain)
		{
			m_swapChain = swapChain;
		}

	private:
		ComPtr<IDXGISwapChain> m_swapChain;
    };
}
