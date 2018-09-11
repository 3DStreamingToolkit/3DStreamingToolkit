#pragma once
#include <winrt\base.h>
#include <winrt\Windows.Storage.Streams.h>
#include <wrl\client.h>
#include <mfmediaengine.h>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <chrono>
#include <windows.graphics.directx.direct3d11.h>
#include <windows.media.core.h>

#define ME_CAN_SEEK 0x00000002

struct CppMediaEngineNotifyCallback : winrt::implements<CppMediaEngineNotifyCallback, winrt::Windows::Foundation::IInspectable>
{
	virtual void OnMediaEngineEvent(DWORD meEvent) = 0;
};

class CppMEPlayer : public winrt::implements<CppMEPlayer, CppMediaEngineNotifyCallback>
{
	// DX11 related
	Microsoft::WRL::ComPtr<ID3D11Device>                m_spDX11Device;
	Microsoft::WRL::ComPtr<ID3D11Device>                m_spDX11ExternalD3DDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>         m_spDX11DeviceContext;
	Microsoft::WRL::ComPtr<IDXGIOutput>                 m_spDXGIOutput;
	Microsoft::WRL::ComPtr<IDXGISwapChain1>             m_spDX11SwapChain;
	Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>        m_spDXGIManager;

	// Media Engine related
	Microsoft::WRL::ComPtr<IMFMediaEngine>              m_spMediaEngine;
	Microsoft::WRL::ComPtr<IMFMediaEngineEx>            m_spEngineEx;

	BSTR												m_bstrURL;
	BOOL												m_fPlaying;
	BOOL												m_fLoop;
	BOOL												m_fEOS;
	BOOL												m_fStopTimer;
	RECT												m_rcTarget;
	MFVideoNormalizedRect								m_nRect;
	DXGI_FORMAT											m_d3dFormat;
	MFARGB												m_bkgColor;

	HANDLE												m_TimerThreadHandle;
	CRITICAL_SECTION									m_critSec;

	//concurrency::task<Windows::Storage::StorageFile^>   m_pickFileTask;
	//concurrency::cancellation_token_source              m_tcs;
	BOOL                                                m_fInitSuccess;
	BOOL                                                m_fExitApp;
	BOOL                                                m_fUseDX;
	BOOL                                                m_fUseVSyncTimer;

public:
	using VideoFrameTransferred = winrt::delegate<CppMEPlayer *const, int, int, Microsoft::WRL::ComPtr<ID3D11Texture2D>, int>;

	CppMEPlayer(Microsoft::WRL::ComPtr<ID3D11Device> unityD3DDevice, BOOL useVSyncTimer = TRUE);

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

	winrt::event<VideoFrameTransferred> FrameTransferred;

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

	//void CloseFilePicker()
	//{
	//	m_tcs.cancel();
	//}

	// Loading
	void SetURL(winrt::hstring szURL);
	void SetBytestream(winrt::Windows::Storage::Streams::IRandomAccessStream const& streamHandle);

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
	void OnVSyncTimer();
	DWORD RealVSyncTimer();
	int GetFrameRate();

	// State related to calculating FPS.
	int m_frameCounter;
	std::chrono::high_resolution_clock::time_point m_lastTimeFPSCalculated;
	int m_renderFps;

	// For calling IDXGIDevice3::Trim() when app is suspended
	HRESULT DXGIDeviceTrim();

private:
	~CppMEPlayer();

	HANDLE m_primarySharedHandle;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
	Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;
};
