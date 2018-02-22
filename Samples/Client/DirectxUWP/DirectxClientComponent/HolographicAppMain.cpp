#include "pch.h"

#include "HolographicAppMain.h"
#include "DirectXHelper.h"

#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>

using namespace DirectXClientComponent;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

// Loads and initializes application assets when the application is loaded.
HolographicAppMain::HolographicAppMain(
	const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
}

void HolographicAppMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

    // Use the default SpatialLocator to track the motion of the device.
    m_locator = SpatialLocator::GetDefault();

    // Be able to respond to changes in the positional tracking state.
    m_locatabilityChangedToken =
        m_locator->LocatabilityChanged +=
            ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
                std::bind(&HolographicAppMain::OnLocatabilityChanged, this, _1, _2)
                );

    // Respond to camera added events by creating any resources that are specific
    // to that camera, such as the back buffer render target view.
    // When we add an event handler for CameraAdded, the API layer will avoid putting
    // the new camera in new HolographicFrames until we complete the deferral we created
    // for that handler, or return from the handler without creating a deferral. This
    // allows the app to take more than one frame to finish creating resources and
    // loading assets for the new holographic camera.
    // This function should be registered before the app creates any HolographicFrames.
    m_cameraAddedToken =
        m_holographicSpace->CameraAdded +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
                std::bind(&HolographicAppMain::OnCameraAdded, this, _1, _2)
                );

    // Respond to camera removed events by releasing resources that were created
	// for that camera.
    // When the app receives a CameraRemoved event, it releases all references to
	// the back buffer right away. This includes render target views, Direct2D 
	// target bitmaps, and so on.
    // The app must also ensure that the back buffer is not attached as a render
	// target, as shown in DeviceResources::ReleaseResourcesForBackBuffer.
    m_cameraRemovedToken =
        m_holographicSpace->CameraRemoved +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
                std::bind(&HolographicAppMain::OnCameraRemoved, this, _1, _2)
                );

    // The simplest way to render world-locked holograms is to create a stationary
	// reference frame when the app is launched. This is roughly analogous to
	// creating a "world" coordinate system with the origin placed at the device's
	// position as the app is launched.
    m_referenceFrame = m_locator->CreateStationaryFrameOfReferenceAtCurrentLocation();

    // Notes on spatial tracking APIs:
    // * Stationary reference frames are designed to provide a best-fit position
	//   relative to the overall space. Individual positions within that reference
	//   frame are allowed to drift slightly as the device learns more about the
	//   environment.
    // * When precise placement of individual holograms is required, a SpatialAnchor 
	//   should be used to anchor the individual hologram to a position in the real
	//   world - for example, a point the user indicates to be of special interest.
	//   Anchor positions do not drift, but can be corrected; the anchor will use the
	//   corrected position starting in the next frame after the correction has
    //   occurred.
}

void HolographicAppMain::SetVideoRender(VideoRenderer* videoRenderer)
{
	m_videoRenderer = videoRenderer;
}

void HolographicAppMain::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        // Clear previous event registrations.

        if (m_cameraAddedToken.Value != 0)
        {
            m_holographicSpace->CameraAdded -= m_cameraAddedToken;
            m_cameraAddedToken.Value = 0;
        }

        if (m_cameraRemovedToken.Value != 0)
        {
            m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
            m_cameraRemovedToken.Value = 0;
        }
    }

    if (m_locator != nullptr)
    {
        m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
    }
}

HolographicAppMain::~HolographicAppMain()
{
    UnregisterHolographicEventHandlers();
}

// Updates the application state once per frame.
HolographicFrame^ HolographicAppMain::Update()
{
    // Before doing the timer update, there is some work to do per-frame
    // to maintain holographic rendering. First, we will get information
    // about the current frame.

    // The HolographicFrame has information that the app needs in order
    // to update and render the current frame. The app begins each new
    // frame by calling CreateNextFrame.
    HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

    // Get a prediction of where holographic cameras will be when this frame
    // is presented.
    HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

    // Back buffers can change from frame to frame. Validate each buffer, and recreate
    // resource views and depth buffers as needed.
    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

	if (m_videoRenderer)
	{
		// Next, we get a coordinate system from the attached frame of reference that is
		// associated with the current frame. Later, this coordinate system is used for
		// for creating the stereo view matrices when rendering the sample content.
		SpatialCoordinateSystem^ currentCoordinateSystem =
			m_referenceFrame->CoordinateSystem;

		// We complete the frame update by using information about our content positioning
		// to set the focus point.

		for (auto cameraPose : prediction->CameraPoses)
		{
			// The HolographicCameraRenderingParameters class provides access to set
			// the image stabilization parameters.
			HolographicCameraRenderingParameters^ renderingParameters =
				holographicFrame->GetRenderingParameters(cameraPose);

			// SetFocusPoint informs the system about a specific point in your scene to
			// prioritize for image stabilization. The focus point is set independently
			// for each holographic camera.
			// You should set the focus point near the content that the user is looking at.
			// In this example, we put the focus point at the center of the sample hologram,
			// since that is the only hologram available for the user to focus on.
			// You can also set the relative velocity and facing of that content; the sample
			// hologram is at a fixed point so we only need to indicate its position.
			renderingParameters->SetFocusPoint(
				currentCoordinateSystem,
				m_videoRenderer->GetFocusPoint());
		}
	}

    // The holographic frame will be used to get up-to-date view and
	// projection matrices and to present the swap chain.
    return holographicFrame;
}

// Renders the current frame to each holographic camera, according to the
// current application and spatial positioning state. Returns true if the
// frame was rendered to at least one camera.
bool HolographicAppMain::Render(
	Windows::Graphics::Holographic::HolographicFrame^ holographicFrame,
	int fps,
	int latency)
{
    //
    // TODO: Add code for pre-pass rendering here.
    //
    // Take care of any tasks that are not specific to an individual holographic
    // camera. This includes anything that doesn't need the final view or projection
    // matrix, such as lighting maps.
    //

    // Lock the set of holographic camera resources, then draw to each camera
    // in this frame.
    return m_deviceResources->UseHolographicCameraResources<bool>(
        [this, holographicFrame, fps, latency](
			std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
    {
        // Up-to-date frame predictions enhance the effectiveness of 
		// image stablization and allow more accurate positioning of holograms.
        //holographicFrame->UpdateCurrentPrediction();
        HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

        bool atLeastOneCameraRendered = false;
        for (auto cameraPose : prediction->CameraPoses)
        {
            // This represents the device-based resources for a HolographicCamera.
            DX::CameraResources* pCameraResources = 
				cameraResourceMap[cameraPose->HolographicCamera->Id].get();

            // Get the device context.
            const auto context = m_deviceResources->GetD3DDeviceContext();
            const auto depthStencilView = pCameraResources->GetDepthStencilView();

            // Set render targets to the current holographic camera.
            ID3D11RenderTargetView *const targets[1] = 
			{ 
				pCameraResources->GetBackBufferRenderTargetView() 
			};

            context->OMSetRenderTargets(1, targets, depthStencilView);

            // Clear the back buffer and depth stencil view.
            context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
            context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            //
            // TODO: Replace the sample content with your own content.
            //
            // Notes regarding holographic content:
            //    * For drawing, remember that you have the potential to fill twice
			//      as many pixels in a stereoscopic render target as compared to a
			//      non-stereoscopic render target of the same resolution. Avoid
			//      unnecessary or repeated writes to the same pixel, and only draw
			//      holograms that the user can see.
            //    * To help occlude hologram geometry, you can create a depth map
			//      using geometry data obtained via the surface mapping APIs.
			//      You can use this depth map to avoid rendering holograms that are
			//      intended to be hidden behind tables, walls, monitors, and so on.
            //    * Black pixels will appear transparent to the user wearing the
			//      device, but you should still use alpha blending to draw
			//      semitransparent holograms. You should also clear the screen to
			//      Transparent as shown above.
            //

            // The projection matrice for each holographic camera will
			// change every frame. This function refreshes the data in the constant
			// buffer for the holographic camera indicated by cameraPose.
            pCameraResources->UpdateProjectionBuffer(
				m_deviceResources,
				cameraPose,
				m_referenceFrame->CoordinateSystem);

            // Attach the projection constant buffer for this camera to the
			// graphics pipeline.
            bool cameraActive = pCameraResources->AttachProjectionBuffer(
				m_deviceResources);

            // Only render world-locked content when positional tracking is active.
            if (cameraActive)
            {
                // Draw the sample hologram.
				m_videoRenderer->Render(fps, latency);
            }

            atLeastOneCameraRendered = true;
        }

        return atLeastOneCameraRendered;
    });
}

SpatialStationaryFrameOfReference^ HolographicAppMain::GetReferenceFrame()
{
	return m_referenceFrame;
}

void HolographicAppMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
{
    switch (sender->Locatability)
    {
    case SpatialLocatability::Unavailable:
        // Holograms cannot be rendered.
        {
            String^ message = L"Warning! Positional tracking is " +
                                        sender->Locatability.ToString() + L".\n";
            OutputDebugStringW(message->Data());
        }
        break;

    // In the following three cases, it is still possible to place holograms using a
    // SpatialLocatorAttachedFrameOfReference.
    case SpatialLocatability::PositionalTrackingActivating:
        // The system is preparing to use positional tracking.

    case SpatialLocatability::OrientationOnly:
        // Positional tracking has not been activated.

    case SpatialLocatability::PositionalTrackingInhibited:
        // Positional tracking is temporarily inhibited. User action may be required
        // in order to restore positional tracking.
        break;

    case SpatialLocatability::PositionalTrackingActive:
        // Positional tracking is active. World-locked content can be rendered.
        break;
    }
}

void HolographicAppMain::OnCameraAdded(
    HolographicSpace^ sender,
    HolographicSpaceCameraAddedEventArgs^ args
    )
{
    Deferral^ deferral = args->GetDeferral();
    HolographicCamera^ holographicCamera = args->Camera;
    create_task([this, deferral, holographicCamera] ()
    {
        //
        // TODO: Allocate resources for the new camera and load any content specific to
        //       that camera. Note that the render target size (in pixels) is a property
        //       of the HolographicCamera object, and can be used to create off-screen
        //       render targets that match the resolution of the HolographicCamera.
        //

        // Create device-based resources for the holographic camera and add it to the list of
        // cameras used for updates and rendering. Notes:
        //   * Since this function may be called at any time, the AddHolographicCamera function
        //     waits until it can get a lock on the set of holographic camera resources before
        //     adding the new camera. At 60 frames per second this wait should not take long.
        //   * A subsequent Update will take the back buffer from the RenderingParameters of this
        //     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
        //     Content can then be rendered for the HolographicCamera.
        m_deviceResources->AddHolographicCamera(holographicCamera);

        // Holographic frame predictions will not include any information about this camera until
        // the deferral is completed.
        deferral->Complete();
    });
}

void HolographicAppMain::OnCameraRemoved(
    HolographicSpace^ sender,
    HolographicSpaceCameraRemovedEventArgs^ args
    )
{
    create_task([this]()
    {
        //
        // TODO: Asynchronously unload or deactivate content resources (not back buffer 
        //       resources) that are specific only to the camera that was removed.
        //
    });

    // Before letting this callback return, ensure that all references to the back buffer 
    // are released.
    // Since this function may be called at any time, the RemoveHolographicCamera function
    // waits until it can get a lock on the set of holographic camera resources before
    // deallocating resources for this camera. At 60 frames per second this wait should
    // not take long.
    m_deviceResources->RemoveHolographicCamera(args->Camera);
}
