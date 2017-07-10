//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "Unity/PlatformBase.h"
#include "MediaPlayerPlayback.h"
#include "meplayer.h"

using namespace Microsoft::WRL;

static UnityGfxRenderer s_DeviceType = kUnityGfxRenderernullptr;
static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;

static float g_Time;

static ComPtr<IMediaEnginePlayback> s_spMediaPlayback;
static MEPlayer^ m_player;

STDAPI_(BOOL) DllMain(
    _In_opt_ HINSTANCE hInstance, _In_ DWORD dwReason, _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    if (DLL_PROCESS_ATTACH == dwReason)
    {
        //  Don't need per-thread callbacks
        DisableThreadLibraryCalls(hInstance);

        Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Create();
    }
    else if (DLL_PROCESS_DETACH == dwReason)
    {
        Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate();
    }

    return TRUE;
}

STDAPI DllGetActivationFactory(_In_ HSTRING activatibleClassId, _COM_Outptr_ IActivationFactory** factory)
{
    auto &module = Microsoft::WRL::Module< Microsoft::WRL::InProc>::GetModule();
    return module.GetActivationFactory(activatibleClassId, factory);
}

STDAPI DllCanUnloadNow()
{
    const auto &module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
    return module.GetObjectCount() == 0 ? S_OK : S_FALSE;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateMediaPlayback()
{
	Log(Log_Level_Info, L"CMediaEnginePlayer::CreateMediaPlayback()");

	if (nullptr == s_UnityInterfaces)
		return;

	if (s_DeviceType == kUnityGfxRendererD3D11)
	{
		IUnityGraphicsD3D11* d3d = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
		m_player = ref new MEPlayer(d3d->GetDevice());
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
    //if (s_spMediaPlayback != nullptr)
    //    LOG_RESULT(s_spMediaPlayback->GetPrimaryTexture(width, height, playbackSRV));

	if (m_player != nullptr)
	    m_player->GetPrimaryTexture(width, height, playbackSRV);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API LoadMediaSource(Windows::Media::Core::IMediaSource^ mediaSourceHandle)
{
	ABI::Windows::Media::Core::IMediaSource * source = reinterpret_cast<ABI::Windows::Media::Core::IMediaSource *>(mediaSourceHandle);

	if (source != nullptr && m_player != nullptr)
	{
		m_player->SetMediaSource(source);
	}
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API LoadContent(_In_ LPCWSTR pszContentLocation)
{
    //if (s_spMediaPlayback != nullptr)
    //    LOG_RESULT(s_spMediaPlayback->LoadContent(pszContentLocation));

	if (m_player != nullptr)
		m_player->SetMediaSourceFromPath(pszContentLocation);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API LoadAdaptiveContent(_In_ LPCWSTR pszManifestLocation)
{
    if (s_spMediaPlayback != nullptr)
        LOG_RESULT(s_spMediaPlayback->LoadAdaptiveContent(pszManifestLocation));
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Play()
{
    //if (s_spMediaPlayback != nullptr)
    //    LOG_RESULT(s_spMediaPlayback->Play());

	if (m_player != nullptr)
		m_player->Play();
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
        s_DeviceType = kUnityGfxRenderernullptr;
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


// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
    return OnRenderEvent;
}

