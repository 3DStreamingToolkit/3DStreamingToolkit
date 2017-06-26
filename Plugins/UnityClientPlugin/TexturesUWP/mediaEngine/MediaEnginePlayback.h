#pragma once

#define ME_CAN_SEEK 0x00000002

#include "MediaHelpers.h"

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

DECLARE_INTERFACE_IID_(IMediaEnginePlayback, IUnknown, "9669c78e-42c4-4178-a1e3-75b03d0f8c9a")
{
    STDMETHOD(GetPrimaryTexture)(_In_ UINT32 width, _In_ UINT32 height, _COM_Outptr_ void** primarySRV) PURE;
    STDMETHOD(LoadContent)(_In_ LPCWSTR pszContentLocation) PURE;
	STDMETHOD(LoadMediaSource)(_In_ Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource> mediaSource) PURE;
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

    HRESULT RuntimeClassInitialize(_In_ ID3D11Device* d3dDevice);

    // IMFMediaEngineNotify  
    IFACEMETHOD(EventNotify)(_In_ DWORD event, _In_ DWORD_PTR param1, _In_ DWORD param2);

    // IMediaPlayerPlayback
    IFACEMETHOD(GetPrimaryTexture)(
        _In_ UINT32 width,
        _In_ UINT32 height,
        _COM_Outptr_ void** primarySRV);

    IFACEMETHOD(LoadContent)(
        _In_ LPCWSTR pszUrl);
	IFACEMETHOD(LoadMediaSource)(
		_In_ Microsoft::WRL::ComPtr<ABI::Windows::Media::Core::IMediaSource> mediaSource);

    IFACEMETHOD(LoadAdaptiveContent)(
        _In_ LPCWSTR pszUrl);

    IFACEMETHOD(Play)();
    IFACEMETHOD(Pause)();
    IFACEMETHOD(Stop)();

    // IFACEMETHOD(AddPlaybackStateCallback)(StateChangedCallback callback) { NULL_CHK(callback); m_fnStateCallback = callback; return S_OK; }

    HRESULT GetCurrentTime();

protected:
    HRESULT OnOpened();
    HRESULT OnEnded();
    HRESULT OnFailed();
    HRESULT OnStateChanged();

private:
    HRESULT CreateMediaEngine();
    void ReleaseMediaEngine();

    HRESULT CreateTextures();
    void ReleaseTextures();

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
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_mediaDeviceContext;

    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> m_mfDXGIMananger;

    // StateChangedCallback m_fnStateCallback;
    Microsoft::WRL::ComPtr<IMFMediaEngine> m_mediaEngine;
    Microsoft::WRL::ComPtr<IMFMediaEngineEx> m_mediaEngineEx;

    PlaybackState m_playerState;

    CD3D11_TEXTURE2D_DESC m_textureDesc;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;

    HANDLE m_primarySharedHandle;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;

    double m_playbackTime;
    bool m_stopTimer;
    bool m_eos;
};
