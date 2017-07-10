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

#include <mfmediaengine.h>


enum class StateType : UINT16
{
	StateType_None = 0,
	StateType_Opened,
	StateType_StateChanged,
	StateType_Failed,
};

enum class PlaybackState : UINT16
{
	PlaybackState_None = 0,
	PlaybackState_Opening,
	PlaybackState_Buffering,
	PlaybackState_Playing,
	PlaybackState_Paused,
	PlaybackState_Ended
};

typedef struct _MEDIA_DESCRIPTION
{
	UINT32 width;
	UINT32 height;
	INT64 duration;
	byte canSeek;
} MEDIA_DESCRIPTION;

typedef struct _PLAYBACK_STATE
{
	StateType type;
	union {
		PlaybackState state;
		HRESULT hresult;
		MEDIA_DESCRIPTION description;
	} value;
} PLAYBACK_STATE;

typedef struct _PLAYER_STATE
{
	PLAYBACK_STATE newState;
	PLAYBACK_STATE oldState;
} PLAYER_STATE;

DECLARE_INTERFACE_IID_(IMediaEnginePlayback, IUnknown, "98a1b0bb-03eb-4935-ae7c-93c1fa0e1c93")
{
    STDMETHOD(GetPrimaryTexture)(_In_ UINT32 width, _In_ UINT32 height, _COM_Outptr_ void** primarySRV) PURE;
    STDMETHOD(LoadContent)(_In_ LPCWSTR pszContentLocation) PURE;
    STDMETHOD(LoadAdaptiveContent)(_In_ LPCWSTR pszManifestLocation) PURE;
    STDMETHOD(Play)() PURE;
    STDMETHOD(Pause)() PURE;
    STDMETHOD(Stop)() PURE;
};

class CMediaEnginePlayback
	: public Microsoft::WRL::RuntimeClass
	< Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
	, IMFMediaEngineNotify
	, IMediaEnginePlayback
	, Microsoft::WRL::FtmBase>
{
public:
	static HRESULT CreateMediaPlayback(_In_ UnityGfxRenderer apiType, _In_ IUnityInterfaces* interfaces, _COM_Outptr_ IMediaEnginePlayback** ppMediaPlayback);

	CMediaEnginePlayback();
    ~CMediaEnginePlayback();

    HRESULT RuntimeClassInitialize(
        _In_ ID3D11Device* d3dDevice);

	// IMFMediaEngineNotify  
	IFACEMETHOD(EventNotify)(_In_ DWORD event, _In_ DWORD_PTR param1, _In_ DWORD param2);

    // IMediaPlayerPlayback
    IFACEMETHOD(GetPrimaryTexture)(
        _In_ UINT32 width, 
        _In_ UINT32 height, 
        _COM_Outptr_ void** primarySRV);

    IFACEMETHOD(LoadContent)(
        _In_ LPCWSTR pszUrl);
    IFACEMETHOD(LoadAdaptiveContent)(
        _In_ LPCWSTR pszUrl);

    IFACEMETHOD(Play)();
    IFACEMETHOD(Pause)();
    IFACEMETHOD(Stop)();

	HRESULT GetCurrentTime();


protected:
	HRESULT OnOpened();
	HRESULT OnEnded();
	HRESULT OnFailed();
	HRESULT OnStateChanged();

private:
    HRESULT CreateMediaPlayer();
    void ReleaseMediaPlayer();

    HRESULT CreateTextures();
	HRESULT CreateLocalTextures();
    void ReleaseTextures();

    HRESULT AddStateChanged();
    void RemoveStateChanged();

    void ReleaseResources();
	HRESULT StartTimer();
	void StopTimer();
	DWORD VSyncTimer();

	HRESULT OnVideoFrameAvailable();

private:
	Microsoft::WRL::Wrappers::CriticalSection m_lock;
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

    Microsoft::WRL::ComPtr<ID3D11Device> m_mediaDevice;
	Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> m_mfDXGIMananger;


    //Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlayer> m_mediaPlayer;
	Microsoft::WRL::ComPtr<IMFMediaEngine> m_mediaEngine;
	Microsoft::WRL::ComPtr<IMFMediaEngineEx> m_mediaEngineEx;

    EventRegistrationToken m_openedEventToken;
    EventRegistrationToken m_endedEventToken;
    EventRegistrationToken m_failedEventToken;
    EventRegistrationToken m_videoFrameAvailableToken;

    PLAYBACK_STATE m_newPlaybackState;
    PLAYBACK_STATE m_oldPlaybackState;

    Microsoft::WRL::ComPtr<ABI::Windows::Media::Playback::IMediaPlaybackSession> m_mediaPlaybackSession;
    EventRegistrationToken m_stateChangedEventToken;

    CD3D11_TEXTURE2D_DESC m_textureDesc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryLocalTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryLocalTextureSRV;

    HANDLE m_primarySharedHandle;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;

	HANDLE m_primaryLocalSharedHandle;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryLocalMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryLocalMediaSurface;

    // TODO: support stereo
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stereoTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_stereoTextureSRV;

    HANDLE m_stereoSharedHandle;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stereoMediaTexture;
    Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_stereoMediaSurface;

	PlaybackState m_playerState;

	double m_playbackTime;
	bool m_stopTimer;
	bool m_eos;
};

