
#include "pch.h"
#include "TexturesUWP.h"
#include <cmath>
#include <d3d11.h>
#include <assert.h>
#include "IUnityGraphics.h"
#include "IUnityInterface.h"
#include "IUnityGraphicsD3D11.h"
#include <thread>

#include "MediaEngine/MediaEnginePlayback.h"

using namespace Microsoft::WRL;

static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;

static float g_Time;

static ComPtr<IMediaEnginePlayback> s_spMediaPlayback;

static ID3D11Device* m_Device;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateMediaPlayback()
{
	Microsoft::WRL::ComPtr<IMediaEnginePlayback> spPlayerPlayback;
	if (SUCCEEDED(CMediaEnginePlayback::CreateMediaPlayback(s_DeviceType, s_UnityInterfaces, &spPlayerPlayback)))
	{
		LOG_RESULT(spPlayerPlayback.As(&s_spMediaPlayback));
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseMediaPlayback()
{
	if (s_spMediaPlayback != nullptr)
	{
		s_spMediaPlayback->Stop();

		s_spMediaPlayback.Reset();
		s_spMediaPlayback = nullptr;
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetPrimaryTexture(_In_ UINT32 width, _In_ UINT32 height, _COM_Outptr_ void** playbackSRV)
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->GetPrimaryTexture(width, height, playbackSRV));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API LoadContent(_In_ LPCWSTR pszContentLocation)
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->LoadContent(pszContentLocation));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API LoadAdaptiveContent(_In_ LPCWSTR pszManifestLocation)
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->LoadAdaptiveContent(pszManifestLocation));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Play()
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->Play());
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Pause()
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->Pause());
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Stop()
{
	if (s_spMediaPlayback != nullptr)
		LOG_RESULT(s_spMediaPlayback->Stop());
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

// GraphicsDeviceEvent
static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		s_DeviceType = s_Graphics->GetRenderer();
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		s_DeviceType = kUnityGfxRendererNull;
	}
}

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}


// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity(float t)
{
	g_Time = t;
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.
static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
}

static void LoadMediaSource(Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource> mediaSource)
{
	s_spMediaPlayback->LoadMediaSource(mediaSource);
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}



