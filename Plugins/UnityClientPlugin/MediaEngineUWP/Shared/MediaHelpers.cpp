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

#pragma once

#include "MediaEngine.h"
#include "MediaHelpers.h"

#include <windows.graphics.holographic.h>

using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace ABI::Windows::Media::Streaming::Adaptive;

using namespace Microsoft::WRL;

#if defined(_DEBUG)
// Check for SDK Layer support.
inline bool SdkLayersAvailable()
{
	D3D_FEATURE_LEVEL featureLevelSupported;
	// featureLevelSupported = D3D_FEATURE_LEVEL_9_3;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
        0,
        D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
        nullptr,                    // Any feature level will do.
        0,
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        nullptr,                    // No need to keep the D3D device reference.
		nullptr,                    // No need to know the feature level.
        nullptr                     // No need to keep the D3D device context reference.
    );

    return SUCCEEDED(hr);
}
#endif

_Use_decl_annotations_
HRESULT CreateMediaSource(
    LPCWSTR pszUrl,
    IMediaSource2** ppMediaSource)
{
    NULL_CHK(pszUrl);
    NULL_CHK(ppMediaSource);

    *ppMediaSource = nullptr;

    // convert the uri
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> spUriFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        &spUriFactory));

    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> spUri;
    IFR(spUriFactory->CreateUri(
        Microsoft::WRL::Wrappers::HStringReference(pszUrl).Get(),
        &spUri));

    // create a media source
    Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSourceStatics> spMediaSourceStatics;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaSource).Get(),
        &spMediaSourceStatics));

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource2> spMediaSource2;
    IFR(spMediaSourceStatics->CreateFromUri(
        spUri.Get(),
        &spMediaSource2));

    *ppMediaSource = spMediaSource2.Detach();

    return S_OK;
}

HRESULT CreateMediaSource2FromMediaSource(ABI::Windows::Media::Core::IMediaSource * mediaSource, ABI::Windows::Media::Core::IMediaSource2 ** ppMediaSource)
{
	NULL_CHK(mediaSource);
	NULL_CHK(ppMediaSource);

	*ppMediaSource = nullptr;

	// create a media source
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSourceStatics> spMediaSourceStatics;
	IFR(ABI::Windows::Foundation::GetActivationFactory(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaSource).Get(),
		&spMediaSourceStatics));

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource2> spMediaSource2;
	IFR(spMediaSourceStatics->CreateFromIMediaSource(
		mediaSource,
		&spMediaSource2));

	*ppMediaSource = spMediaSource2.Detach();

	return S_OK;
}

_Use_decl_annotations_
HRESULT CreateMediaSource2FromMediaStreamSource(ABI::Windows::Media::Core::IMediaStreamSource * mediaSource, ABI::Windows::Media::Core::IMediaSource2 ** ppMediaSource)
{
	NULL_CHK(mediaSource);
	NULL_CHK(ppMediaSource);

	*ppMediaSource = nullptr;

	// create a media source
	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSourceStatics> spMediaSourceStatics;
	IFR(ABI::Windows::Foundation::GetActivationFactory(
		Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Media_Core_MediaSource).Get(),
		&spMediaSourceStatics));

	Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource2> spMediaSource2;
	IFR(spMediaSourceStatics->CreateFromMediaStreamSource(
		mediaSource,
		&spMediaSource2));

	*ppMediaSource = spMediaSource2.Detach();

	return S_OK;
}
HRESULT CreateAdaptiveMediaSource(
    LPCWSTR pszManifestLocation,
    IAdaptiveMediaSourceCompletedCallback* pCallback)
{
    NULL_CHK(pszManifestLocation);
    NULL_CHK(pCallback);

    ComPtr<IAdaptiveMediaSourceCompletedCallback> spCallback(pCallback);

    // convert the uri
    ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> spUriFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
        &spUriFactory));

    ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> spUri;
    IFR(spUriFactory->CreateUri(
        Wrappers::HStringReference(pszManifestLocation).Get(),
        &spUri));

    // get factory for adaptive source
    ComPtr<IAdaptiveMediaSourceStatics> spAdaptiveSourceStatics;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Streaming_Adaptive_AdaptiveMediaSource).Get(),
        &spAdaptiveSourceStatics));

    // get the asyncOp for creating the source
    ComPtr<ICreateAdaptiveMediaSourceOperation> asyncOp;
    IFR(spAdaptiveSourceStatics->CreateFromUriAsync(spUri.Get(), &asyncOp));

    // create a completed callback
    auto completedHandler = Microsoft::WRL::Callback<ICreateAdaptiveMediaSourceResultHandler>(
        [spCallback, asyncOp](_In_ ICreateAdaptiveMediaSourceOperation *pOp, _In_ AsyncStatus status) -> HRESULT
    {
        return spCallback->OnAdaptiveMediaSourceCreated(pOp, status);
    });

    IFR(asyncOp->put_Completed(completedHandler.Get()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreatePlaylistSource(
    IMediaSource2* pSource,
    IMediaPlaybackSource** ppMediaPlaybackSource)
{
    NULL_CHK(pSource);
    NULL_CHK(ppMediaPlaybackSource);

    *ppMediaPlaybackSource = nullptr;

    ComPtr<IMediaPlaybackList> spPlaylist;
    IFR(Windows::Foundation::ActivateInstance(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackList).Get(),
        &spPlaylist));

    // get the iterator for playlist
    ComPtr<ABI::Windows::Foundation::Collections::IObservableVector<MediaPlaybackItem*>> spItems;
    IFR(spPlaylist->get_Items(&spItems));

    ComPtr<ABI::Windows::Foundation::Collections::IVector<MediaPlaybackItem*>> spItemsVector;
    IFR(spItems.As(&spItemsVector));

    // create playbackitem from source
    ComPtr<IMediaPlaybackItemFactory> spItemFactory;
    IFR(ABI::Windows::Foundation::GetActivationFactory(
        Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackItem).Get(),
        &spItemFactory));

    ComPtr<IMediaPlaybackItem> spItem;
    IFR(spItemFactory->Create(pSource, &spItem));

    // add to the list
    IFR(spItemsVector->Append(spItem.Get()));

    // convert to playbackSource
    ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
    IFR(spPlaylist.As(&spMediaPlaybackSource));

    *ppMediaPlaybackSource = spMediaPlaybackSource.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetSurfaceFromTexture(
    ID3D11Texture2D* pTexture,
    IDirect3DSurface** ppSurface)
{
    NULL_CHK(pTexture);
    NULL_CHK(ppSurface);

    *ppSurface = nullptr;

    ComPtr<ID3D11Texture2D> spTexture(pTexture);

    ComPtr<IDXGISurface> dxgiSurface;
    IFR(spTexture.As(&dxgiSurface));

    ComPtr<IInspectable> inspectableSurface;
    IFR(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.Get(), &inspectableSurface));

    ComPtr<IDirect3DSurface> spSurface;
    IFR(inspectableSurface.As(&spSurface));

    *ppSurface = spSurface.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT GetTextureFromSurface(
    IDirect3DSurface* pSurface,
    ID3D11Texture2D** ppTexture)
{
    NULL_CHK(pSurface);
    NULL_CHK(ppTexture);

    *ppTexture = nullptr;

    ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> spDXGIInterfaceAccess;
    IFR(pSurface->QueryInterface(IID_PPV_ARGS(&spDXGIInterfaceAccess)));
    IFR(spDXGIInterfaceAccess->GetInterface(IID_PPV_ARGS(ppTexture)));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CreateMediaDevice(
    IDXGIAdapter* pDXGIAdapter, 
    ID3D11Device** ppDevice,
	ID3D11DeviceContext** ppContext)
{
    NULL_CHK(ppDevice);

    // Create the Direct3D 11 API device object and a corresponding context.
    D3D_FEATURE_LEVEL featureLevel;

    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    if (SdkLayersAvailable())
    {
        // If the project is in a debug build, enable debugging via SDK Layers with this flag.
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
#endif
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    ComPtr<ID3D11Device> spDevice;
    ComPtr<ID3D11DeviceContext> spContext;

    D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_WARP;

    // Create a device using the hardware graphics driver if adapter is not supplied
    HRESULT hr = D3D11CreateDevice(
		nullptr,               // if nullptr will use default adapter.
        driverType, 
		nullptr,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
        creationFlags,              // Set debug and Direct2D compatibility flags.
        featureLevels,              // List of feature levels this app can support.
        ARRAYSIZE(featureLevels),   // Size of the list above.
        D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        &spDevice,                  // Returns the Direct3D device created.
        &featureLevel,              // Returns feature level of device created.
        &spContext                  // Returns the device immediate context.
    );

    if (FAILED(hr))
    {
        // fallback to WARP if we are not specifying an adapter
        if (nullptr == pDXGIAdapter)
        {
            // If the initialization fails, fall back to the WARP device.
            // For more information on WARP, see: 
            // http://go.microsoft.com/fwlink/?LinkId=286690
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                0,
                creationFlags,
                featureLevels,
                ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION,
                &spDevice,
                &featureLevel,
                &spContext);
        }

        IFR(hr);
    }
    else
    {
        // workaround for nvidia GPU's, cast to ID3D11VideoDevice
        Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice;
        spDevice.As(&videoDevice);
    }

    // Turn multithreading on 
    ComPtr<ID3D10Multithread> spMultithread;
    if (SUCCEEDED(spContext.As(&spMultithread)))
    {
        spMultithread->SetMultithreadProtected(TRUE);
    }

    *ppDevice = spDevice.Detach();
	*ppContext = spContext.Detach();

    return S_OK;
}
