//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
#pragma once

#include <wrl.h>
#include <mfmediaengine.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <ppltasks.h>
#include <agile.h>

#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

using namespace std::chrono;

#ifndef MEPLAYER_H
#define MEPLAYER_H

#define ME_CAN_SEEK 0x00000002

namespace MEDIA
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch DirectX API errors
            throw Platform::Exception::CreateException(hr);
        }
    }
}

//-----------------------------------------------------------------------------
// MediaEngineNotifyCallback
//
// Defines the callback method to process media engine events.
//-----------------------------------------------------------------------------
ref struct MediaEngineNotifyCallback abstract
{
internal:
    virtual void OnMediaEngineEvent(DWORD meEvent) = 0;
};

// MEPlayer: Manages the Media Engine.

ref class MEPlayer: public MediaEngineNotifyCallback
{
    // DX11 related
    Microsoft::WRL::ComPtr<ID3D11Device>                m_spDX11Device;
	Microsoft::WRL::ComPtr<ID3D11Device>                m_spDX11UnityDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>         m_spDX11DeviceContext;
    Microsoft::WRL::ComPtr<IDXGIOutput>                 m_spDXGIOutput;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_spDX11SwapChain;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>        m_spDXGIManager;

    // Media Engine related
    Microsoft::WRL::ComPtr<IMFMediaEngine>              m_spMediaEngine;
    Microsoft::WRL::ComPtr<IMFMediaEngineEx>            m_spEngineEx;

    BSTR                                    m_bstrURL;
    BOOL                                    m_fPlaying;
    BOOL                                    m_fLoop;
    BOOL                                    m_fEOS;
    BOOL                                    m_fStopTimer;
    RECT                                    m_rcTarget;
	MFVideoNormalizedRect					m_nRect;
    DXGI_FORMAT                             m_d3dFormat;
    MFARGB                                  m_bkgColor;

    HANDLE                                  m_TimerThreadHandle;
    CRITICAL_SECTION                        m_critSec;

    concurrency::task<Windows::Storage::StorageFile^>   m_pickFileTask;
    concurrency::cancellation_token_source              m_tcs;
    BOOL                                                m_fInitSuccess;    
    BOOL                                                m_fExitApp;
    BOOL                                                m_fUseDX;

internal:

	delegate void VideoFrameTransferred(MEPlayer^ sender, int width, int height);

    MEPlayer(Microsoft::WRL::ComPtr<ID3D11Device> unityD3DDevice);

    // DX11 related
    void CreateDX11Device();
    void CreateBackBuffers();

    // Initialize/Shutdown
    void Initialize(float width, float height);
    void Shutdown();
    BOOL ExitApp();	

	HRESULT GetPrimaryTexture(
		UINT32 width,
		UINT32 height,
		_COM_Outptr_ void** primarySRV);

	HRESULT GetPrimary2DTexture(
		UINT32 width,
		UINT32 height,
		_COM_Outptr_ ID3D11Texture2D** primaryTexture);

	HRESULT SetMediaSourceFromPath(LPCWSTR pszContentLocation);
	HRESULT SetMediaSource(ABI::Windows::Media::Core::IMediaSource* mediaSource);
	HRESULT SetMediaStreamSource(ABI::Windows::Media::Core::IMediaStreamSource* mediaSource);

    // Media Engine related
    virtual void OnMediaEngineEvent(DWORD meEvent) override;

	event VideoFrameTransferred^ FrameTransferred;

    // Media Engine related Options
    void AutoPlay(BOOL autoPlay)
    {
        if (m_spMediaEngine)
        {
            m_spMediaEngine->SetAutoPlay(autoPlay);
        }
    }

    void Loop()
    {
        if (m_spMediaEngine)
        {   
            (m_fLoop) ? m_fLoop = FALSE : m_fLoop = TRUE;
            m_spMediaEngine->SetLoop(m_fLoop);
        }
    }

    BOOL IsPlaying()
    {
        return m_fPlaying;
    }

    void CloseFilePicker()
    {
        m_tcs.cancel();
    }

    // Loading
    void SetURL(Platform::String^ szURL);  
    void SetBytestream(Windows::Storage::Streams::IRandomAccessStream^ streamHandle);

    // Transport state
    void Play();
    void Pause();
    void FrameStep();

    // Audio
    void SetVolume(float fVol);
    void SetBalance(float fBal);
    void Mute(BOOL mute);

    // Video
    void ResizeVideo(HWND hwnd);
    void EnableVideoEffect(BOOL enable);

    // Seeking and duration.
    void GetDuration(double *pDuration, BOOL *pbCanSeek);
    void SetPlaybackPosition(float pos);    
    double  GetPlaybackPosition();    
    BOOL    IsSeeking();

    // Window Event Handlers
    void UpdateForWindowSizeChange(float width, float height);

    // Timer thread related
	void UpdateFrameRate();
    void StartTimer();
    void StopTimer();	
    void OnTimer();
    DWORD RealVSyncTimer();

	// State related to calculating FPS.
	int _frameCounter;
	high_resolution_clock::time_point _lastTimeFPSCalculated;

    // For calling IDXGIDevice3::Trim() when app is suspended
    HRESULT DXGIDeviceTrim();

private:
    ~MEPlayer();

	HANDLE m_primarySharedHandle;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;

};

#endif /* MEPLAYER_H */
