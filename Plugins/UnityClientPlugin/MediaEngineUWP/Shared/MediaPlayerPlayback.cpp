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

#include "MediaEngine.h"
#include "MediaPlayerPlayback.h"
#include "MediaHelpers.h"

using namespace Microsoft::WRL;
using namespace ABI::Windows::Graphics::DirectX::Direct3D11;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Media::Core;
using namespace ABI::Windows::Media::Playback;
using namespace Windows::Foundation;

HRESULT GetDXGIOutput(
	_In_ ID3D11Device* pDevice,
	_COM_Outptr_ IDXGIOutput** ppOutput)
{
	NULL_CHK(pDevice);
	NULL_CHK(ppOutput);

	*ppOutput = nullptr;

	ComPtr<ID3D11Device> spDevice(pDevice);

	ComPtr<IDXGIDevice> spDXGIDevice;
	IFR(spDevice.As(&spDXGIDevice));

	ComPtr<IDXGIAdapter> spAdapter;
	IFR(spDXGIDevice->GetAdapter(&spAdapter));

	// default first output
	ComPtr<IDXGIOutput> spOutput;
	IFR(spAdapter->EnumOutputs(0, &spOutput));

	*ppOutput = spOutput.Detach();

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::CreateMediaPlayback(
    UnityGfxRenderer apiType, 
    IUnityInterfaces* interfaces, 
	IMediaEnginePlayback** ppMediaPlayback)
{
	Log(Log_Level_Info, L"CMediaEnginePlayer::CreateMediaPlayback()");

	if (nullptr == interfaces || nullptr == ppMediaPlayback)
		IFR(E_INVALIDARG);

	if (apiType == kUnityGfxRendererD3D11)
	{
		IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
		NULL_CHK_HR(d3d, E_INVALIDARG);

		ComPtr<CMediaEnginePlayback> spMediaPlayback(nullptr);
		IFR(MakeAndInitialize<CMediaEnginePlayback>(&spMediaPlayback, d3d->GetDevice()));

		*ppMediaPlayback = spMediaPlayback.Detach();
	}
	else
	{
		IFR(E_INVALIDARG);
	}

	return S_OK;
}


_Use_decl_annotations_
CMediaEnginePlayback::CMediaEnginePlayback()
    : m_d3dDevice(nullptr)
    , m_mediaDevice(nullptr)
	, m_d3dContext(nullptr)
	, m_mediaEngine(nullptr)
    , m_mediaPlaybackSession(nullptr)
    , m_primaryTexture(nullptr)
    , m_primaryTextureSRV(nullptr)
    , m_primarySharedHandle(INVALID_HANDLE_VALUE)
    , m_primaryMediaTexture(nullptr)
    , m_primaryMediaSurface(nullptr)
    , m_stereoTexture(nullptr)
    , m_stereoTextureSRV(nullptr)
    , m_stereoSharedHandle(INVALID_HANDLE_VALUE)
    , m_stereoMediaTexture(nullptr)
    , m_stereoMediaSurface(nullptr)
	, m_mfDXGIMananger(nullptr)
	, m_stopTimer(true)
	, m_eos(false)
	, m_playbackTime(0.0)
	, m_playerState(PlaybackState::PlaybackState_None)
{
}

_Use_decl_annotations_
CMediaEnginePlayback::~CMediaEnginePlayback()
{
    ReleaseTextures();

    ReleaseMediaPlayer();

    MFUnlockDXGIDeviceManager();

    ReleaseResources();
}


_Use_decl_annotations_
HRESULT CMediaEnginePlayback::RuntimeClassInitialize(
    ID3D11Device* d3dDevice)
{
	Log(Log_Level_Info, L"CMediaEnginePlayback::RuntimeClassInitialize()");

	if (nullptr == d3dDevice)
		IFR(E_INVALIDARG);

	IFR(MFStartup(MF_VERSION));

	// ref count passed in device
	ComPtr<ID3D11Device> spDevice(d3dDevice);

	// make sure creation of the device is on the same adapter
	ComPtr<IDXGIDevice> spDXGIDevice;
	IFR(spDevice.As(&spDXGIDevice));

	ComPtr<IDXGIAdapter> spAdapter;
	IFR(spDXGIDevice->GetAdapter(&spAdapter));

	// create dx device for media pipeline
	ComPtr<ID3D11Device> spMediaDevice;
	ComPtr<ID3D11DeviceContext> spMediaContext;
	IFR(CreateMediaDevice(spAdapter.Get(), &spMediaDevice, &spMediaContext));

	// lock the shared dxgi device manager
	// will keep lock open for the life of object
	//     call MFUnlockDXGIDeviceManager when unloading
	UINT uiResetToken;
	ComPtr<IMFDXGIDeviceManager> spDeviceManager;
	IFR(MFLockDXGIDeviceManager(&uiResetToken, &spDeviceManager));

	// associtate the device with the manager
	IFR(spDeviceManager->ResetDevice(spMediaDevice.Get(), uiResetToken));

	// create media plyaer object
	IFR(CreateMediaPlayer());

	// save the devices
	m_d3dDevice.Attach(spDevice.Detach());
	m_d3dContext.Attach(spMediaContext.Detach());
	m_mediaDevice.Attach(spMediaDevice.Detach());
	m_mfDXGIMananger.Attach(spDeviceManager.Detach());

	// create the video texture description based on texture format
	m_textureDesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, 1920, 1080);
	m_textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	m_textureDesc.MipLevels = 1;
	m_textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
	m_textureDesc.Usage = D3D11_USAGE_DEFAULT;

	CreateLocalTextures();

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::GetPrimaryTexture(
    UINT32 width,
    UINT32 height,
    void** srv)
{
    if (width < 1 || height < 1 || nullptr == srv)
        IFR(E_INVALIDARG);

    IFR(CreateTextures());

    //ComPtr<ID3D11ShaderResourceView> spSRV;
    //IFR(m_primaryTextureSRV.CopyTo(&spSRV));

    //*srv = spSRV.Detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::LoadContent(
    LPCWSTR pszContentLocation)
{
	Log(Log_Level_Info, L"CMediaEnginePlayback::LoadContent()");

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
	IFR(m_mediaEngine.As(&spEngineSource));

	ComPtr<IMediaPlayerSource> spMediaPlayerSource;
	IFR(m_mediaEngine.As(&spMediaPlayerSource));

	IFR(spEngineSource->SetPlaybackSource(spMediaPlaybackSource.Get()));

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::LoadAdaptiveContent(
    _In_ LPCWSTR pszAdaptiveLocation)
{
    return LoadContent(pszAdaptiveLocation);
}


_Use_decl_annotations_
HRESULT CMediaEnginePlayback::Play()
{
	Log(Log_Level_Info, L"CMediaEnginePlayback::Play()");

	if (nullptr == m_mediaEngine)
		IFR(E_FAIL);

	if (m_eos)
	{
		m_mediaEngine->SetCurrentTime(0);
	}

	if (m_stopTimer)
	{
		// Start the Timer thread
		IFR(StartTimer());
	}

	m_eos = FALSE;

	IFR(m_mediaEngine->Play());

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::Pause()
{
	Log(Log_Level_Info, L"CMediaEnginePlayback::Play()");

	if (nullptr != m_mediaEngine)
	{
		IFR(m_mediaEngine->Pause());
	}

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::Stop()
{
	auto lock = m_lock.Lock();

	if (nullptr != m_mediaEngine)
	{
		StopTimer();

		ComPtr<IMediaEnginePlaybackSource> spEngineSource;
		IFR(m_mediaEngine.As(&spEngineSource));
		IFR(spEngineSource->SetPlaybackSource(nullptr));
	}

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::CreateMediaPlayer()
{
	// Set factory configuration for engine
	Microsoft::WRL::ComPtr<IMFAttributes> mfAttributes;
	IFR(MFCreateAttributes(&mfAttributes, 1));

	ComPtr<IUnknown> thisUnk;
	IFR(this->QueryInterface(IID_PPV_ARGS(&thisUnk)));

	IFR(mfAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, thisUnk.Get()));

	IFR(mfAttributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, m_mfDXGIMananger.Get()));

	IFR(mfAttributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM));

	// create media engine factory
	ComPtr<IMFMediaEngineClassFactory> mediaEngineFactory;
	IFR(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&mediaEngineFactory)));

	DWORD flags = 0;
	flags |= MF_MEDIA_ENGINE_CREATEFLAGS::MF_MEDIA_ENGINE_REAL_TIME_MODE;
	flags |= MF_MEDIA_ENGINE_CREATEFLAGS::MF_MEDIA_ENGINE_WAITFORSTABLE_STATE;

	// create media engine
	IFR(mediaEngineFactory->CreateInstance(flags, mfAttributes.Get(), &m_mediaEngine));
	IFR(m_mediaEngine.As(&m_mediaEngineEx));

	return S_OK;
}

_Use_decl_annotations_
void CMediaEnginePlayback::ReleaseMediaPlayer()
{
	if (m_mediaEngine != nullptr)
	{
		m_mediaEngine->Shutdown();
	}
	m_mediaEngine.Reset();

	m_mediaEngine = nullptr;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::CreateTextures()
{
    if (nullptr != m_primaryTexture || nullptr != m_primaryTextureSRV)
        ReleaseTextures();

    // create staging texture on unity device
    ComPtr<ID3D11Texture2D> spTexture;
    IFR(m_d3dDevice->CreateTexture2D(&m_textureDesc, nullptr, &spTexture));

    auto srvDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D);
    ComPtr<ID3D11ShaderResourceView> spSRV;
    IFR(m_d3dDevice->CreateShaderResourceView(spTexture.Get(), &srvDesc, &spSRV));

    // create shared texture from the unity texture
    ComPtr<IDXGIResource1> spDXGIResource;
    IFR(spTexture.As(&spDXGIResource));

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
        hr = m_mediaDevice.As(&spMediaDevice);
        if (SUCCEEDED(hr))
        {
            hr = spMediaDevice->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(&spMediaTexture));
            if (SUCCEEDED(hr))
            {
                hr = GetSurfaceFromTexture(spMediaTexture.Get(), &spMediaSurface);
            }
        }
    }

    // if anything failed, clean up the handle
    if (FAILED(hr))
    {
        CloseHandle(sharedHandle);
        IFR(hr);
    }

    m_primaryTexture.Attach(spTexture.Detach());
    m_primaryTextureSRV.Attach(spSRV.Detach());

    m_primarySharedHandle = sharedHandle;
    m_primaryMediaTexture.Attach(spMediaTexture.Detach());
    m_primaryMediaSurface.Attach(spMediaSurface.Detach());

    return hr;
}

HRESULT CMediaEnginePlayback::CreateLocalTextures()
{
	// create staging texture on local media device
	ComPtr<ID3D11Texture2D> spLocalTexture;
	IFR(m_mediaDevice->CreateTexture2D(&m_textureDesc, nullptr, &spLocalTexture));

	auto srvLocalDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(spLocalTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D);
	ComPtr<ID3D11ShaderResourceView> spLocalSRV;
	IFR(m_mediaDevice->CreateShaderResourceView(spLocalTexture.Get(), &srvLocalDesc, &spLocalSRV));

	// create shared texture from the unity texture
	ComPtr<IDXGIResource1> spLocalDXGIResource;
	IFR(spLocalTexture.As(&spLocalDXGIResource));

	HANDLE sharedLocalHandle;
	ComPtr<ID3D11Texture2D> spLocalMediaTexture;
	ComPtr<IDirect3DSurface> spLocalMediaSurface;
	HRESULT hr = spLocalDXGIResource->CreateSharedHandle(
		nullptr,
		DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
		L"SharedLocalTextureHandle",
		&sharedLocalHandle);
	if (SUCCEEDED(hr))
	{
		ComPtr<ID3D11Device1> spLocalMediaDevice;
		hr = m_mediaDevice.As(&spLocalMediaDevice);
		if (SUCCEEDED(hr))
		{
			hr = spLocalMediaDevice->OpenSharedResource1(sharedLocalHandle, IID_PPV_ARGS(&spLocalMediaTexture));
			if (SUCCEEDED(hr))
			{
				hr = GetSurfaceFromTexture(spLocalMediaTexture.Get(), &spLocalMediaSurface);
			}
		}
	}
	
	m_primaryLocalSharedHandle = sharedLocalHandle;
	m_primaryLocalMediaTexture.Attach(spLocalMediaTexture.Detach());
	m_primaryLocalMediaSurface.Attach(spLocalMediaSurface.Detach());

	m_primaryLocalTexture.Attach(spLocalTexture.Detach());
	m_primaryLocalTextureSRV.Attach(spLocalSRV.Detach());

	return hr;
}

_Use_decl_annotations_
void CMediaEnginePlayback::ReleaseTextures()
{
    Log(Log_Level_Info, L"CMediaEnginePlayback::ReleaseTextures()");

    // primary texture
    if (m_primarySharedHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_primarySharedHandle);
        m_primarySharedHandle = INVALID_HANDLE_VALUE;
    }

    m_primaryMediaSurface.Reset();
    m_primaryMediaSurface = nullptr;

    m_primaryMediaTexture.Reset();
    m_primaryMediaTexture = nullptr;

    m_primaryTextureSRV.Reset();
    m_primaryTextureSRV = nullptr;

    m_primaryTexture.Reset();
    m_primaryTexture = nullptr;

    // stereo texture
    if (m_stereoSharedHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_stereoSharedHandle);
        m_stereoSharedHandle = INVALID_HANDLE_VALUE;
    }

    m_stereoMediaSurface.Reset();
    m_stereoMediaSurface = nullptr;

    m_stereoMediaTexture.Reset();
    m_stereoMediaTexture = nullptr;

    m_stereoTextureSRV.Reset();
    m_primaryTextureSRV = nullptr;

    m_stereoTexture.Reset();
    m_stereoTexture = nullptr;

    ZeroMemory(&m_textureDesc, sizeof(m_textureDesc));
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::AddStateChanged()
{
    //ComPtr<IMediaPlayer3> spMediaPlayer3;
    //IFR(m_mediaPlayer.As(&spMediaPlayer3));

    //ComPtr<IMediaPlaybackSession> spSession;
    //IFR(spMediaPlayer3->get_PlaybackSession(&spSession));

    //EventRegistrationToken stateChangedToken;
    //auto stateChanged = Microsoft::WRL::Callback<IMediaPlaybackSessionEventHandler>(this, &CMediaEnginePlayback::OnStateChanged);
    //IFR(spSession->add_PlaybackStateChanged(stateChanged.Get(), &stateChangedToken));
    //m_stateChangedEventToken = stateChangedToken;
    //m_mediaPlaybackSession.Attach(spSession.Detach());

    return S_OK;
}

_Use_decl_annotations_
void CMediaEnginePlayback::RemoveStateChanged()
{
    // remove playback session callbacks
    if (nullptr != m_mediaPlaybackSession)
    {
        LOG_RESULT(m_mediaPlaybackSession->remove_PlaybackStateChanged(m_stateChangedEventToken));

        m_mediaPlaybackSession.Reset();
        m_mediaPlaybackSession = nullptr;
    }
}

_Use_decl_annotations_
void CMediaEnginePlayback::ReleaseResources()
{
    // release dx devices
    m_mediaDevice.Reset();
    m_mediaDevice = nullptr;

    m_d3dDevice.Reset();
    m_d3dDevice = nullptr;
}

HRESULT CMediaEnginePlayback::StartTimer()
{
	ComPtr<ABI::Windows::System::Threading::IThreadPoolStatics> threadPoolStatics;
	IFR(Windows::Foundation::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_System_Threading_ThreadPool).Get(),
		&threadPoolStatics));

	ComPtr<CMediaEnginePlayback> spThis(this);
	auto workItem =
		Microsoft::WRL::Callback<ABI::Windows::System::Threading::IWorkItemHandler>(
			[this, spThis](ABI::Windows::Foundation::IAsyncAction* asyncAction) -> HRESULT
	{
		m_stopTimer = false;

		spThis->VSyncTimer();

		return S_OK;
	});

	ComPtr<ABI::Windows::Foundation::IAsyncAction> spAction;
	IFR(threadPoolStatics->RunWithPriorityAsync(
		workItem.Get(),
		ABI::Windows::System::Threading::WorkItemPriority::WorkItemPriority_High,
		&spAction));

	return S_OK;
}

void CMediaEnginePlayback::StopTimer()
{
	m_stopTimer = true;
	m_eos = false;
}

DWORD CMediaEnginePlayback::VSyncTimer()
{
	ComPtr<IDXGIOutput> spOutput;

	while (true)
	{
		if (m_stopTimer)
		{
			break;
		}

		LONGLONG pts;
		if (SUCCEEDED(m_mediaEngine->OnVideoStreamTick(&pts)))
		{
			LOG_RESULT(OnVideoFrameAvailable());
		}

		if (nullptr == spOutput)
		{
			LOG_RESULT(GetDXGIOutput(m_d3dDevice.Get(), &spOutput));

			if (nullptr == spOutput)
			{
				m_stopTimer = true;
			}
		}

		if (FAILED(spOutput->WaitForVBlank()))
		{
			spOutput.Reset(); // just in-case the output was removed

			Sleep(16);
		}
	}

	return 0;
}

//Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryTexture;
//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_primaryTextureSRV;
//
//HANDLE m_primarySharedHandle;
//Microsoft::WRL::ComPtr<ID3D11Texture2D> m_primaryMediaTexture;
//Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> m_primaryMediaSurface;


HRESULT CMediaEnginePlayback::OnVideoFrameAvailable()
{
	auto lock = m_lock.Lock();

	if (nullptr != m_primaryLocalMediaTexture)
	{
		MFVideoNormalizedRect sourceRect = { 0.0f, 0.0f, 1.0f, 1.0f };
		RECT destinationRect = { 0, 0, static_cast<LONG>(m_textureDesc.Width), static_cast<LONG>(m_textureDesc.Height) };

		const MFARGB borderColor = { 0, 0, 0, 0xff };

		IFR(m_mediaEngine->TransferVideoFrame(m_primaryLocalMediaTexture.Get(), nullptr, &destinationRect, &borderColor));
	}

	return S_OK;
}

//_Use_decl_annotations_
//HRESULT CMediaEnginePlayback::OnVideoFrameAvailable(IMediaPlayer* sender, IInspectable* arg)
//{
//    ComPtr<IMediaPlayer> spMediaPlayer(sender);
//
//    ComPtr<IMediaPlayer5> spMediaPlayer5;
//    IFR(spMediaPlayer.As(&spMediaPlayer5));
//
//    if (nullptr != m_primaryMediaSurface)
//    {
//        if (nullptr != m_stereoMediaSurface)
//        {
//            // TODO: use shared textures, this will fail in UWP builds
//            // create a new shared texture for the secondary and use that here
//            IFR(spMediaPlayer5->CopyFrameToStereoscopicVideoSurfaces(m_primaryMediaSurface.Get(), m_stereoMediaSurface.Get()));
//        }
//        else
//        {
//            IFR(spMediaPlayer5->CopyFrameToVideoSurface(m_primaryMediaSurface.Get()));
//        }
//    }
//
//    return S_OK;
//}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::GetCurrentTime()
{
	if (nullptr != m_mediaEngine)
	{
		m_playbackTime = m_mediaEngine->GetCurrentTime();
	}
	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2)
{
	Log(Log_Level_Info, L"CMediaEnginePlayback::EventNotify()");

	// https://msdn.microsoft.com/en-us/library/windows/desktop/hh162842%28v=vs.85%29.aspx

	if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
	{
		// handle if the engine was created with  MF_MEDIA_ENGINE_WAITFORSTABLE_STATE
		SetEvent(reinterpret_cast<HANDLE>(param1));
	}
	else
	{
		switch (meEvent)
		{
		case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
			m_playerState = PlaybackState::PlaybackState_Opening;
			m_eos = false;
			break;

		case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
			m_playerState = PlaybackState::PlaybackState_Opening;
			break;

		case MF_MEDIA_ENGINE_EVENT_WAITING:
			m_playerState = PlaybackState::PlaybackState_Buffering;
			break;

		case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
			return OnOpened();
			break;

		case MF_MEDIA_ENGINE_EVENT_CANPLAY:
			m_playerState = PlaybackState::PlaybackState_Paused;
			break;

		case MF_MEDIA_ENGINE_EVENT_PLAY:
			m_playerState = PlaybackState::PlaybackState_Playing;
			break;

		case MF_MEDIA_ENGINE_EVENT_PLAYING:
			m_playerState = PlaybackState::PlaybackState_Playing;
			break;

		case MF_MEDIA_ENGINE_EVENT_PAUSE:
			m_playerState = PlaybackState::PlaybackState_Paused;
			break;

		case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
			m_playerState = PlaybackState::PlaybackState_Opening;
			break;

		case MF_MEDIA_ENGINE_EVENT_ENDED:
			m_playerState = PlaybackState::PlaybackState_Ended;
			m_eos = true;
			return OnEnded();
			break;

		case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
			GetCurrentTime();
			break;

		case MF_MEDIA_ENGINE_EVENT_ABORT:
		case MF_MEDIA_ENGINE_EVENT_ERROR:
			m_playerState = PlaybackState::PlaybackState_None;
			return OnFailed();
			break;
		}
	}

	return OnStateChanged();
}


_Use_decl_annotations_
HRESULT CMediaEnginePlayback::OnOpened()
{
	if (nullptr == m_mediaEngine)
		return E_NOT_VALID_STATE;

	DWORD width, height;
	IFR(m_mediaEngine->GetNativeVideoSize(&width, &height));

	ComPtr<IMFMediaTimeRange> spRange;
	IFR(m_mediaEngine->GetSeekable(&spRange));
	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_Opened;
	playbackState.value.description.width = width;
	playbackState.value.description.height = height;
	playbackState.value.description.canSeek = (spRange != nullptr);
	playbackState.value.description.duration = (spRange != nullptr) ? spRange->GetLength() : 0;

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::OnEnded()
{
	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_StateChanged;
	playbackState.value.state = PlaybackState::PlaybackState_Ended;

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::OnFailed()
{
	HRESULT hr = E_UNEXPECTED;
	if (nullptr != m_mediaEngine)
	{
		Microsoft::WRL::ComPtr<IMFMediaError> error;
		m_mediaEngine->GetError(&error);
		if (error != nullptr)
		{
			USHORT errorCode = error->GetErrorCode();
			Log(Log_Level_Error, L"MF_MEDIA_ENGINE_EVENT_ERROR code: %d\n", errorCode);
			(void)errorCode;

			hr = error->GetExtendedErrorCode();
			LOG_RESULT_MSG(hr, L"Error during playback");
		}
	}

	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_Failed;
	playbackState.value.hresult = hr;

	return S_OK;
}

_Use_decl_annotations_
HRESULT CMediaEnginePlayback::OnStateChanged()
{
	PLAYBACK_STATE playbackState;
	ZeroMemory(&playbackState, sizeof(playbackState));
	playbackState.type = StateType::StateType_StateChanged;
	playbackState.value.state = m_playerState;

	return S_OK;
}
