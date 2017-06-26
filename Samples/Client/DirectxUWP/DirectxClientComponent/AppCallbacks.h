#pragma once

#include "pch.h"
#include "DeviceResources.h"
#include "VideoRenderer.h"
#include "HolographicAppMain.h"

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;

namespace DirectXClientComponent
{
	public delegate void SendInputDataHandler(String^ msg);

    public ref class AppCallbacks sealed
    {
    public:
		AppCallbacks(SendInputDataHandler^ sendInputDataHandler);
		virtual ~AppCallbacks();

		void Initialize(CoreApplicationView^ appView);

		void SetWindow(CoreWindow^ window);

		void Run();

		void OnFrame(
			uint32_t width,
			uint32_t height,
			const Array<uint8_t>^ dataY,
			uint32_t strideY,
			const Array<uint8_t>^ dataU,
			uint32_t strideU,
			const Array<uint8_t>^ dataV,
			uint32_t strideV);

		void OnDecodedFrame(
			uint32_t width,
			uint32_t height,
			const Array<uint8_t>^ decodedData);

	private:
		void SendInputData(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

		std::shared_ptr<DX::DeviceResources>					m_deviceResources;
		VideoRenderer*											m_videoRenderer;
		SendInputDataHandler^									m_sendInputDataHandler;

		bool													m_isHolographic;
		std::unique_ptr<HolographicAppMain>						m_main;
		
		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace^		m_holographicSpace;
    };
}
