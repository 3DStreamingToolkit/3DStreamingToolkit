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

#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.media.core.h>
#include <windows.media.playback.h>
#include <windows.media.streaming.adaptive.h>

typedef ABI::Windows::Foundation::IAsyncOperation<ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSourceCreationResult*> ICreateAdaptiveMediaSourceOperation;
typedef ABI::Windows::Foundation::IAsyncOperationCompletedHandler<ABI::Windows::Media::Streaming::Adaptive::AdaptiveMediaSourceCreationResult*> ICreateAdaptiveMediaSourceResultHandler;

HRESULT CreateMediaSource(
    _In_ LPCWSTR pszUrl,
    _COM_Outptr_ ABI::Windows::Media::Core::IMediaSource2** ppMediaSource);

HRESULT CreatePlaylistSource(
    _In_ ABI::Windows::Media::Core::IMediaSource2* pSource,
    _COM_Outptr_ ABI::Windows::Media::Playback::IMediaPlaybackSource** ppMediaPlaybackSource);

HRESULT GetSurfaceFromTexture(
    _In_ ID3D11Texture2D* pTexture,
    _COM_Outptr_ ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface** ppSurface);

HRESULT GetTextureFromSurface(
    _In_ ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface* pSurface,
    _COM_Outptr_ ID3D11Texture2D** ppTexture);

HRESULT CreateMediaDevice(
    _In_opt_ IDXGIAdapter* pDXGIAdapter,
    _COM_Outptr_ ID3D11Device** ppDevice);
