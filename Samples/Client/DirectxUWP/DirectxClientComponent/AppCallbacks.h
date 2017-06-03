#pragma once

#include "pch.h"
#include "DeviceResources.h"
#include "VideoRenderer.h"
#include "VideoDecoder.h"
#ifdef HOLOLENS
#include "HolographicAppMain.h"
#endif // HOLOLENS

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;

namespace DirectXClientComponent
{
    public ref class AppCallbacks sealed
    {
    public:
		AppCallbacks();
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

		void OnEncodedFrame(
			uint32_t width,
			uint32_t height,
			const Array<uint8_t>^ encodedData);

	private:
		void ReadI420Buffer(
			int width,
			int height,
			uint8* buffer,
			uint8** dataY,
			int* strideY,
			uint8** dataU,
			int* strideU,
			uint8** dataV,
			int* strideV);

		std::shared_ptr<DX::DeviceResources>					m_deviceResources;
		VideoRenderer*											m_videoRenderer;
		VideoDecoder*											m_videoDecoder;

		bool													m_isHolographic;
#ifdef HOLOLENS
		std::unique_ptr<HolographicAppMain>						m_main;
		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace^		m_holographicSpace;
#endif // HOLOLENS
    };
}
