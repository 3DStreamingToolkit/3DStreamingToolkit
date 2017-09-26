#pragma once

#include "pch.h"
#include "DeviceResources.h"
#include "VideoRenderer.h"
#include "HolographicAppMain.h"
#include "MediaEnginePlayer.h"

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
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

		void SetMediaStreamSource(Windows::Media::Core::IMediaStreamSource^ mediaSourceHandle);

	private:
		void InitVideoRender(std::shared_ptr<DX::DeviceResources> deviceResources, ID3D11ShaderResourceView* textureView);
		void SendInputData(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame);

		std::shared_ptr<DX::DeviceResources>					m_deviceResources;
		VideoRenderer*											m_videoRenderer;
		MEPlayer^												m_player;
		std::unique_ptr<HolographicAppMain>						m_main;
		SendInputDataHandler^									m_sendInputDataHandler;
		bool													m_sentStereoMode;
		ABI::Windows::Media::Core::IMediaStreamSource *			m_mediaSource;
		
		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace^		m_holographicSpace;
    };
}
