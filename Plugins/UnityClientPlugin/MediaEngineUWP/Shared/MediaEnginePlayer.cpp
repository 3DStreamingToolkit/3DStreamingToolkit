//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//

#include "MediaEngine.h"
#include "MediaEnginePlayer.h"
#include "MediaHelpers.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace Windows::System::Threading;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace concurrency;

using namespace Microsoft::WRL;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace Windows::Foundation;

// MediaEngineNotify: Implements the callback for Media Engine event notification.
class MediaEngineNotify : public IMFMediaEngineNotify
{
	long m_cRef;
	Platform::Agile<Windows::UI::Core::CoreWindow> m_cWindow;
	MediaEngineNotifyCallback^ m_pCB;

public:
	MediaEngineNotify(Platform::Agile<Windows::UI::Core::CoreWindow> cWindow) : m_cWindow(cWindow), m_cRef(1), m_pCB(nullptr)
	{
	}

	STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
	{
		if (__uuidof(IMFMediaEngineNotify) == riid)
		{
			*ppv = static_cast<IMFMediaEngineNotify*>(this);
		}
		else
		{
			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		AddRef();

		return S_OK;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&m_cRef);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		LONG cRef = InterlockedDecrement(&m_cRef);
		if (cRef == 0)
		{
			delete this;
		}
		return cRef;
	}

	// EventNotify is called when the Media Engine sends an event.
	STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2)
	{
		if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
		{
			SetEvent(reinterpret_cast<HANDLE>(param1));
		}
		else
		{
			m_pCB->OnMediaEngineEvent(meEvent);
		}

		return S_OK;
	}

	void MediaEngineNotifyCallback(MediaEngineNotifyCallback^ pCB)
	{
		m_pCB = pCB;
	}
};

MEPlayer::MEPlayer(Microsoft::WRL::ComPtr<ID3D11Device> unityD3DDevice) :
	m_spDX11Device(nullptr),
	m_spDX11UnityDevice(unityD3DDevice),
	m_spDX11DeviceContext(nullptr),
	m_spDXGIOutput(nullptr),
	m_spDX11SwapChain(nullptr),
	m_spDXGIManager(nullptr),
	m_spMediaEngine(nullptr),
	m_spEngineEx(nullptr),
	m_bstrURL(nullptr),
	m_TimerThreadHandle(nullptr),
	m_fPlaying(FALSE),
	m_fLoop(FALSE),
	m_fEOS(FALSE),
	m_fStopTimer(TRUE),
	m_d3dFormat(DXGI_FORMAT_B8G8R8A8_UNORM),
	m_fInitSuccess(FALSE),
	m_fExitApp(FALSE),
	m_fUseDX(TRUE)
{
	memset(&m_bkgColor, 0, sizeof(MFARGB));

	InitializeCriticalSectionEx(&m_critSec, 0, 0);
}

MEPlayer::~MEPlayer()
{
	Shutdown();

	MFShutdown();

	DeleteCriticalSection(&m_critSec);
}

//+-------------------------------------------------------------------------
//
//  Function:   CreateDX11Device()
//
//  Synopsis:   creates a default device
//
//--------------------------------------------------------------------------
void MEPlayer::CreateDX11Device()
{
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	D3D_FEATURE_LEVEL FeatureLevel;
	HRESULT hr = S_OK;

	if (m_fUseDX)
	{
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			levels,
			ARRAYSIZE(levels),
			D3D11_SDK_VERSION,
			&m_spDX11Device,
			&FeatureLevel,
			&m_spDX11DeviceContext
		);
	}

	//Failed to create DX11 Device (using VM?), create device using WARP
	if (FAILED(hr))
	{
		m_fUseDX = FALSE;
	}

	if (!m_fUseDX)
	{
		MEDIA::ThrowIfFailed(D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			levels,
			ARRAYSIZE(levels),
			D3D11_SDK_VERSION,
			&m_spDX11Device,
			&FeatureLevel,
			&m_spDX11DeviceContext
		));
	}

	if (m_fUseDX)
	{
		ComPtr<ID3D10Multithread> spMultithread;
		MEDIA::ThrowIfFailed(
			m_spDX11Device.Get()->QueryInterface(IID_PPV_ARGS(&spMultithread))
		);
		spMultithread->SetMultithreadProtected(TRUE);
	}

	return;
}

//+-----------------------------------------------------------------------------
//
//  Function:   CreateBackBuffers
//
//  Synopsis:   Creates the D3D back buffers
//
//------------------------------------------------------------------------------
void MEPlayer::CreateBackBuffers()
{
	EnterCriticalSection(&m_critSec);

	// make sure everything is released first;    
	if (m_spDX11Device)
	{
		// Acquire the DXGIdevice lock 
		CAutoDXGILock DXGILock(m_spDXGIManager);

		ComPtr<ID3D11Device> spDevice;
		MEDIA::ThrowIfFailed(
			DXGILock.LockDevice(/*out*/spDevice)
		);

		// long QI chain to get DXGIFactory from the device
		ComPtr<IDXGIDevice2> spDXGIDevice;
		MEDIA::ThrowIfFailed(
			spDevice.Get()->QueryInterface(IID_PPV_ARGS(&spDXGIDevice))
		);

		// Ensure that DXGI does not queue more than one frame at a time. This both reduces 
		// latency and ensures that the application will only render after each VSync, minimizing 
		// power consumption.
		MEDIA::ThrowIfFailed(
			spDXGIDevice->SetMaximumFrameLatency(1)
		);

		// create the video texture description based on texture format
		auto m_textureDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, m_rcTarget.right, m_rcTarget.bottom);
		m_textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		m_textureDesc.MipLevels = 1;
		m_textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
		m_textureDesc.Usage = D3D11_USAGE_DEFAULT;

		// create staging texture on unity device
		ComPtr<ID3D11Texture2D> spTexture;
		MEDIA::ThrowIfFailed(m_spDX11UnityDevice->CreateTexture2D(&m_textureDesc, nullptr, &spTexture));

		auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D);
		ComPtr<ID3D11ShaderResourceView> spSRV;
		MEDIA::ThrowIfFailed(m_spDX11UnityDevice->CreateShaderResourceView(spTexture.Get(), &srvDesc, &spSRV));

		// create shared texture from the unity texture
		ComPtr<IDXGIResource1> spDXGIResource;
		MEDIA::ThrowIfFailed(spTexture.As(&spDXGIResource));

		HANDLE sharedHandle;
		ComPtr<ID3D11Texture2D> spMediaTexture;
		ComPtr<IDirect3DSurface> spMediaSurface;
		HRESULT hr = spDXGIResource->CreateSharedHandle(
			nullptr,
			DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
			L"SharedTextureHandle",
			&sharedHandle);
		if (SUCCEEDED(hr))
		{
			ComPtr<ID3D11Device1> spMediaDevice;
			hr = spDevice.As(&spMediaDevice);
			if (SUCCEEDED(hr))
			{
				hr = spMediaDevice->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(&spMediaTexture));
				if (SUCCEEDED(hr))
				{
					hr = GetSurfaceFromTexture(spMediaTexture.Get(), &spMediaSurface);
				}
			}
		}

		m_primarySharedHandle = sharedHandle;
		m_primaryMediaTexture.Attach(spMediaTexture.Detach());
		m_primaryMediaSurface.Attach(spMediaSurface.Detach());

		m_primaryTextureSRV.Attach(spSRV.Detach());
	}

	high_resolution_clock::time_point now = high_resolution_clock::now();
	_lastTimeFPSCalculated = now;

	LeaveCriticalSection(&m_critSec);

	return;
}

// Create a new instance of the Media Engine.
void MEPlayer::Initialize(float width, float height)
{
	ComPtr<IMFMediaEngineClassFactory> spFactory;
	ComPtr<IMFAttributes> spAttributes;
	ComPtr<MediaEngineNotify> spNotify;

	HRESULT hr = S_OK;

	// Get the bounding rectangle of the window.
	m_rcTarget.left = 0;
	m_rcTarget.top = 0;
	m_rcTarget.right = width;
	m_rcTarget.bottom = height;

	MEDIA::ThrowIfFailed(MFStartup(MF_VERSION));

	EnterCriticalSection(&m_critSec);

	try
	{

		// Create DX11 device.    
		CreateDX11Device();

		UINT resetToken;
		MEDIA::ThrowIfFailed(
			MFCreateDXGIDeviceManager(&resetToken, &m_spDXGIManager)
		);

		MEDIA::ThrowIfFailed(
			m_spDXGIManager->ResetDevice(m_spDX11Device.Get(), resetToken)
		);

		// Create our event callback object.
		spNotify = new MediaEngineNotify(nullptr);
		if (spNotify == nullptr)
		{
			MEDIA::ThrowIfFailed(E_OUTOFMEMORY);
		}

		spNotify->MediaEngineNotifyCallback(this);

		// Create the class factory for the Media Engine.
		MEDIA::ThrowIfFailed(
			CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spFactory))
		);

		// Set configuration attribiutes.
		MEDIA::ThrowIfFailed(
			MFCreateAttributes(&spAttributes, 1)
		);

		MEDIA::ThrowIfFailed(
			spAttributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown*)m_spDXGIManager.Get())
		);

		MEDIA::ThrowIfFailed(
			spAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, (IUnknown*)spNotify.Get())
		);

		MEDIA::ThrowIfFailed(
			spAttributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, m_d3dFormat)
		);

		// Create the Media Engine.
		const DWORD flags = MF_MEDIA_ENGINE_WAITFORSTABLE_STATE;
		MEDIA::ThrowIfFailed(
			spFactory->CreateInstance(flags, spAttributes.Get(), &m_spMediaEngine)
		);

		MEDIA::ThrowIfFailed(
			m_spMediaEngine.Get()->QueryInterface(__uuidof(IMFMediaEngine), (void**)&m_spEngineEx)
		);

		m_spEngineEx->SetRealTimeMode(TRUE);

		// Create/Update swap chain
		UpdateForWindowSizeChange(width, height);

		m_fInitSuccess = TRUE;

	}
	catch (Platform::Exception^)
	{
	}

	LeaveCriticalSection(&m_critSec);

	return;
}

// Shut down the player and release all interface pointers.
void MEPlayer::Shutdown()
{
	EnterCriticalSection(&m_critSec);

	StopTimer();

	if (m_spMediaEngine)
	{
		m_spMediaEngine->Shutdown();
	}

	if (nullptr != m_bstrURL)
	{
		::CoTaskMemFree(m_bstrURL);
	}

	LeaveCriticalSection(&m_critSec);

	return;
}

// Set a URL
void MEPlayer::SetURL(Platform::String^ szURL)
{
	if (nullptr != m_bstrURL)
	{
		::CoTaskMemFree(m_bstrURL);
		m_bstrURL = nullptr;
	}

	size_t cchAllocationSize = 1 + ::wcslen(szURL->Data());
	m_bstrURL = (LPWSTR)::CoTaskMemAlloc(sizeof(WCHAR)*(cchAllocationSize));

	if (m_bstrURL == 0)
	{
		MEDIA::ThrowIfFailed(E_OUTOFMEMORY);
	}

	StringCchCopyW(m_bstrURL, cchAllocationSize, szURL->Data());

	return;
}

// Set Bytestream
void MEPlayer::SetBytestream(IRandomAccessStream^ streamHandle)
{
	HRESULT hr = S_OK;
	ComPtr<IMFByteStream> spMFByteStream = nullptr;

	MEDIA::ThrowIfFailed(
		MFCreateMFByteStreamOnStreamEx((IUnknown*)streamHandle, &spMFByteStream)
	);

	MEDIA::ThrowIfFailed(
		m_spEngineEx->SetSourceFromByteStream(spMFByteStream.Get(), m_bstrURL)
	);

	return;
}

HRESULT MEPlayer::SetMediaSourceFromPath(LPCWSTR pszContentLocation)
{
	// create the media source for content (fromUri)
	ComPtr<IMediaSource2> spMediaSource2;
	IFR(CreateMediaSource(pszContentLocation, &spMediaSource2));

	// create playbackitem from source
	ComPtr<IMediaPlaybackItemFactory> spItemFactory;
	IFR(ABI::Windows::Foundation::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackItem).Get(),
		&spItemFactory));

	ComPtr<IMediaPlaybackItem> spItem;
	IFR(spItemFactory->Create(spMediaSource2.Get(), &spItem));

	ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
	IFR(spItem.As(&spMediaPlaybackSource));

	ComPtr<IMediaEnginePlaybackSource> spEngineSource;
	IFR(m_spEngineEx.As(&spEngineSource));
	IFR(spEngineSource->SetPlaybackSource(spMediaPlaybackSource.Get()));

	return S_OK;
}

HRESULT MEPlayer::GetPrimaryTexture(UINT32 width, UINT32 height, void ** primarySRV)
{
	if (width < 1 || height < 1 || nullptr == primarySRV)
		IFR(E_INVALIDARG);

	Initialize(width, height);

	ComPtr<ID3D11ShaderResourceView> spSRV;
	IFR(m_primaryTextureSRV.CopyTo(&spSRV));

	*primarySRV = spSRV.Detach();

	return S_OK;
}

HRESULT MEPlayer::GetPrimary2DTexture(UINT32 width, UINT32 height, ID3D11Texture2D ** primaryTexture)
{
	if (width < 1 || height < 1 || nullptr == primaryTexture)
		IFR(E_INVALIDARG);

	Initialize(width, height);

	ComPtr<ID3D11Texture2D> spTexture;
	IFR(m_primaryMediaTexture.CopyTo(&spTexture));

	*primaryTexture = spTexture.Detach();

	return S_OK;
}

HRESULT MEPlayer::SetMediaSource(ABI::Windows::Media::Core::IMediaSource * mediaSource)
{
	 // create the media source for content (from MediaSource)
	 ComPtr<IMediaSource2> spMediaSource2;
	IFR(CreateMediaSource2FromMediaSource(mediaSource, &spMediaSource2));

	// create playbackitem from source
	ComPtr<IMediaPlaybackItemFactory> spItemFactory;
	IFR(ABI::Windows::Foundation::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackItem).Get(),
		&spItemFactory));

	ComPtr<IMediaPlaybackItem> spItem;
	IFR(spItemFactory->Create(spMediaSource2.Detach(), &spItem));

	ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
	IFR(spItem.As(&spMediaPlaybackSource));

	ComPtr<IMediaEnginePlaybackSource> spEngineSource;
	IFR(m_spEngineEx.As(&spEngineSource));
	IFR(spEngineSource->SetPlaybackSource(spMediaPlaybackSource.Get()));

	return S_OK;
}

HRESULT MEPlayer::SetMediaStreamSource(ABI::Windows::Media::Core::IMediaStreamSource * mediaSource)
{
	// create the media source for content (from MediaSource)
	ComPtr<IMediaSource2> spMediaSource2;
	IFR(CreateMediaSource2FromMediaStreamSource(mediaSource, &spMediaSource2));

	// create playbackitem from source
	ComPtr<IMediaPlaybackItemFactory> spItemFactory;
	IFR(ABI::Windows::Foundation::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Media_Playback_MediaPlaybackItem).Get(),
		&spItemFactory));

	ComPtr<IMediaPlaybackItem> spItem;
	IFR(spItemFactory->Create(spMediaSource2.Detach(), &spItem));

	ComPtr<IMediaPlaybackSource> spMediaPlaybackSource;
	IFR(spItem.As(&spMediaPlaybackSource));

	ComPtr<IMediaEnginePlaybackSource> spEngineSource;
	IFR(m_spEngineEx.As(&spEngineSource));
	IFR(spEngineSource->SetPlaybackSource(spMediaPlaybackSource.Get()));

	return S_OK;
}

void MEPlayer::OnMediaEngineEvent(DWORD meEvent)
{
	switch (meEvent)
	{
	case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
	{
		m_fEOS = FALSE;
	}
	break;
	case MF_MEDIA_ENGINE_EVENT_CANPLAY:
	{
		// Start the Playback
		Play();
	}
	break;
	case MF_MEDIA_ENGINE_EVENT_PLAY:
		m_fPlaying = TRUE;
		break;
	case MF_MEDIA_ENGINE_EVENT_PAUSE:
		m_fPlaying = FALSE;
		break;
	case MF_MEDIA_ENGINE_EVENT_ENDED:

		if (m_spMediaEngine->HasVideo())
		{
			StopTimer();
		}
		m_fEOS = TRUE;
		break;
	case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
		break;
	case MF_MEDIA_ENGINE_EVENT_ERROR:
		break;
	}

	return;
}

// Start playback.
void MEPlayer::Play()
{
	if (m_spMediaEngine)
	{
		StartTimer();

		if (m_fEOS)
		{
			SetPlaybackPosition(0);
			m_fPlaying = TRUE;
		}
		else
		{
			MEDIA::ThrowIfFailed(
				m_spMediaEngine->Play()
			);
		}

		m_fEOS = FALSE;
	}
	return;
}

// Pause playback.
void MEPlayer::Pause()
{
	if (m_spMediaEngine)
	{
		MEDIA::ThrowIfFailed(
			m_spMediaEngine->Pause()
		);
	}
	return;
}

// Set the audio volume.
void MEPlayer::SetVolume(float fVol)
{
	if (m_spMediaEngine)
	{
		MEDIA::ThrowIfFailed(
			m_spMediaEngine->SetVolume(fVol)
		);
	}
	return;
}

// Set the audio balance.
void MEPlayer::SetBalance(float fBal)
{
	if (m_spEngineEx)
	{
		MEDIA::ThrowIfFailed(
			m_spEngineEx->SetBalance(fBal)
		);
	}
	return;
}

// Mute the audio.
void MEPlayer::Mute(BOOL mute)
{
	if (m_spMediaEngine)
	{
		MEDIA::ThrowIfFailed(
			m_spMediaEngine->SetMuted(mute)
		);
	}
	return;
}

// Step forward one frame.
void MEPlayer::FrameStep()
{
	if (m_spEngineEx)
	{
		MEDIA::ThrowIfFailed(
			m_spEngineEx->FrameStep(TRUE)
		);
	}
	return;
}

// Get the duration of the content.
void MEPlayer::GetDuration(double *pDuration, BOOL *pbCanSeek)
{
	if (m_spMediaEngine)
	{
		double duration = m_spMediaEngine->GetDuration();

		// NOTE:
		// "duration != duration"
		// This tests if duration is NaN, because NaN != NaN

		if (duration != duration || duration == std::numeric_limits<float>::infinity())
		{
			*pDuration = 0;
			*pbCanSeek = FALSE;
		}
		else
		{
			*pDuration = duration;

			DWORD caps = 0;
			m_spEngineEx->GetResourceCharacteristics(&caps);
			*pbCanSeek = (caps & ME_CAN_SEEK) == ME_CAN_SEEK;
		}
	}
	else
	{
		MEDIA::ThrowIfFailed(E_FAIL);
	}

	return;
}

// Get the current playback position.
double MEPlayer::GetPlaybackPosition()
{
	if (m_spMediaEngine)
	{
		return m_spMediaEngine->GetCurrentTime();
	}
	else
	{
		return 0;
	}
}

// Seek to a new playback position.
void MEPlayer::SetPlaybackPosition(float pos)
{
	if (m_spMediaEngine)
	{
		MEDIA::ThrowIfFailed(
			m_spMediaEngine->SetCurrentTime(pos)
		);
	}
}

// Is the player in the middle of a seek operation?
BOOL MEPlayer::IsSeeking()
{
	if (m_spMediaEngine)
	{
		return m_spMediaEngine->IsSeeking();
	}
	else
	{
		return FALSE;
	}
}

void MEPlayer::EnableVideoEffect(BOOL enable)
{
	HRESULT hr = S_OK;

	if (m_spEngineEx)
	{
		MEDIA::ThrowIfFailed(m_spEngineEx->RemoveAllEffects());
		if (enable)
		{
			ComPtr<IMFActivate> spActivate;
			LPCWSTR szActivatableClassId = WindowsGetStringRawBuffer((HSTRING)Windows::Media::VideoEffects::VideoStabilization->Data(), nullptr);

			MEDIA::ThrowIfFailed(MFCreateMediaExtensionActivate(szActivatableClassId, nullptr, IID_PPV_ARGS(&spActivate)));

			MEDIA::ThrowIfFailed(m_spEngineEx->InsertVideoEffect(spActivate.Get(), FALSE));
		}
	}

	return;
}

// Window Event Handlers
void MEPlayer::UpdateForWindowSizeChange(float width, float height)
{
	// Get the bounding rectangle of the window.    

	m_nRect.top = 0.0f;
	m_nRect.left = 0.0f;
	m_nRect.right = 1.0f;
	m_nRect.bottom = 1.0f;

	m_rcTarget.top = 0.0f;
	m_rcTarget.left = 0.0f;
	m_rcTarget.bottom = height;
	m_rcTarget.right = width;

	if (m_spEngineEx)
	{
		CreateBackBuffers();
	}

	return;
}

//Timer related

void MEPlayer::UpdateFrameRate()
{
	// Do FPS calculation and notification.
	_frameCounter++;

	high_resolution_clock::time_point now = high_resolution_clock::now();
	duration<double, std::milli> time_span = now - _lastTimeFPSCalculated;
	if (time_span.count() > 1000) {

		_RPT1(0, "%d\n", _frameCounter);

		_frameCounter = 0;
		_lastTimeFPSCalculated = now;
	}
}

//+-----------------------------------------------------------------------------
//
//  Function:   StartTimer
//
//  Synopsis:   Our timer is based on the displays VBlank interval
//
//------------------------------------------------------------------------------
void MEPlayer::StartTimer()
{
	ComPtr<IDXGIFactory1> spFactory;
	MEDIA::ThrowIfFailed(
		CreateDXGIFactory1(IID_PPV_ARGS(&spFactory))
	);

	ComPtr<IDXGIAdapter> spAdapter;
	MEDIA::ThrowIfFailed(
		spFactory->EnumAdapters(0, &spAdapter)
	);

	ComPtr<IDXGIOutput> spOutput;
	MEDIA::ThrowIfFailed(
		spAdapter->EnumOutputs(0, &m_spDXGIOutput)
	);

	m_fStopTimer = FALSE;

	auto vidPlayer = this;
	task<void> workItem(ThreadPool::RunAsync(ref new WorkItemHandler([=](IAsyncAction^ /*sender*/) {
		vidPlayer->RealVSyncTimer();
	}
	),
		WorkItemPriority::High
		));

	return;
}

//+-----------------------------------------------------------------------------
//
//  Function:   StopTimer
//
//  Synopsis:   Stops the Timer and releases all its resources
//
//------------------------------------------------------------------------------
void MEPlayer::StopTimer()
{
	m_fStopTimer = TRUE;
	m_fPlaying = FALSE;

	return;
}

//+-----------------------------------------------------------------------------
//
//  Function:   realVSyncTimer
//
//  Synopsis:   A real VSyncTimer - a timer that fires at approx 60 Hz 
//              synchronized with the display's real VBlank interrupt.
//
//------------------------------------------------------------------------------
DWORD MEPlayer::RealVSyncTimer()
{
	for (;; )
	{
		if (m_fStopTimer)
		{
			break;
		}

		if (SUCCEEDED(m_spDXGIOutput->WaitForVBlank()))
		{
			OnTimer();
		}
		else break;
	}

	return 0;
}

//+-----------------------------------------------------------------------------
//
//  Function:   OnTimer
//
//  Synopsis:   Called at 60Hz - we simply call the media engine and draw
//              a new frame to the screen if told to do so.
//
//------------------------------------------------------------------------------
void MEPlayer::OnTimer()
{
	EnterCriticalSection(&m_critSec);

	if (m_spMediaEngine != nullptr)
	{
		LONGLONG pts;
		if (m_spMediaEngine->OnVideoStreamTick(&pts) == S_OK)
		{
			MEDIA::ThrowIfFailed(
				m_spMediaEngine->TransferVideoFrame(m_primaryMediaTexture.Get(), &m_nRect, &m_rcTarget, &m_bkgColor)
			);

			FrameTransferred(this, m_rcTarget.right, m_rcTarget.bottom);
		}
	}

	LeaveCriticalSection(&m_critSec);

	return;
}

//+-----------------------------------------------------------------------------
//
//  Function:   DXGIDeviceTrim
//
//  Synopsis:   Calls IDXGIDevice3::Trim() (requirement when app is suspended)
//
//------------------------------------------------------------------------------
HRESULT MEPlayer::DXGIDeviceTrim()
{
	HRESULT hr = S_OK;
	if (m_fUseDX && m_spDX11Device != nullptr)
	{
		IDXGIDevice3 *pDXGIDevice;
		hr = m_spDX11Device.Get()->QueryInterface(__uuidof(IDXGIDevice3), (void **)&pDXGIDevice);
		if (hr == S_OK)
		{
			pDXGIDevice->Trim();
		}
	}

	return hr;
}

//+-----------------------------------------------------------------------------
//
//  Function:   ExitApp
//
//  Synopsis:   Checks if there has been an error and indicates if the app
//				should exit.
//
//------------------------------------------------------------------------------
BOOL MEPlayer::ExitApp()
{
	return m_fExitApp;
}