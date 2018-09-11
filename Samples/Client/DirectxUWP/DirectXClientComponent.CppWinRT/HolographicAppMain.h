#pragma once
#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"
#include "VideoRenderer.h"
#include <winrt\Windows.Graphics.Holographic.h>
#include <winrt\Windows.Perception.Spatial.h>

namespace DirectXClientComponent_CppWinRT
{
	class HolographicAppMain
	{
	public:
		HolographicAppMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~HolographicAppMain();

		// Sets the holographic space. This is our closest analogue to setting a new window
		// for the app.
		void SetHolographicSpace(
			winrt::Windows::Graphics::Holographic::HolographicSpace const& holographicSpace);

		// Sets the video renderer.
		void SetVideoRender(VideoRenderer* videoRenderer);

		// Starts the holographic frame and updates the content.
		winrt::Windows::Graphics::Holographic::HolographicFrame Update();

		// Renders holograms, including world-locked content.
		bool Render(
			winrt::Windows::Graphics::Holographic::HolographicFrame const& holographicFrame,
			int fps = 0,
			int latency = 0);

		// Gets the reference frame.
		winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference GetReferenceFrame();

	private:
		// Asynchronously creates resources for new holographic cameras.
		void OnCameraAdded(
			winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
			winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args);

		// Synchronously releases resources for holographic cameras that are no longer
		// attached to the system.
		void OnCameraRemoved(
			winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
			winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args);

		// Used to notify the app when the positional tracking state changes.
		void OnLocatabilityChanged(
			winrt::Windows::Perception::Spatial::SpatialLocator const& sender,
			winrt::Windows::Foundation::IInspectable const& args);

		// Clears event registration state. Used when changing to a new HolographicSpace
		// and when tearing down AppMain.
		void UnregisterHolographicEventHandlers();

		// Renders a colorful holographic cube that's 20 centimeters wide. This sample content
		// is used to demonstrate world-locked rendering.
		VideoRenderer*													m_videoRenderer;

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources>                            m_deviceResources;

		// Render loop timer.
		DX::StepTimer                                                   m_timer;

		// Represents the holographic space around the user.
		winrt::Windows::Graphics::Holographic::HolographicSpace               m_holographicSpace;

		// SpatialLocator that is attached to the primary camera.
		winrt::Windows::Perception::Spatial::SpatialLocator                   m_locator;

		// A reference frame attached to the holographic camera.
		winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_referenceFrame;

		// Event registration tokens.
		winrt::event_token                     m_cameraAddedToken;
		winrt::event_token                     m_cameraRemovedToken;
		winrt::event_token                     m_locatabilityChangedToken;
	};
}


