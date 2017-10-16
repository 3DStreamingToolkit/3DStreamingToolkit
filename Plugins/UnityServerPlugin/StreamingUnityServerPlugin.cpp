#define SUPPORT_D3D11 1
#define WEBRTC_WIN 1

#define SHOW_CONSOLE 0


#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <cstdint>
#include <wrl.h>
#include <d3d11_2.h>
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphics.h"
#include "IUnityInterface.h"

#include "buffer_renderer.h"
#include "conductor.h"
#include "server_main_window.h"
#include "flagdefs.h"
#include "config_parser.h"

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"
#include "webrtc/base/logging.h"

#include "turn_credential_provider.h"
#include "server_authentication_provider.h"
#include "peer_connection_client.h"


#pragma warning( disable : 4100 )
#pragma comment(lib, "ws2_32.lib") 

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

#pragma comment(lib, "dmoguids.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "msdmo.lib")
#pragma comment(lib, "strmiids.lib")

#pragma comment(lib, "common_video.lib")
#pragma comment(lib, "webrtc.lib")
#pragma comment(lib, "boringssl_asm.lib")
#pragma comment(lib, "field_trial_default.lib")
#pragma comment(lib, "metrics_default.lib")
#pragma comment(lib, "protobuf_full.lib")

using namespace Microsoft::WRL;
using namespace StreamingToolkit;

DEFINE_GUID(IID_Texture2D, 0x6f15aaf2, 0xd208, 0x4e89, 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c);

static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static ComPtr<ID3D11Device> s_Device;
static ComPtr<ID3D11DeviceContext> s_Context;

static rtc::scoped_refptr<Conductor> s_conductor = nullptr;

static BufferRenderer*		s_bufferRenderer = nullptr;
static ID3D11Texture2D*		s_frameBuffer = nullptr;
static WebRTCConfig			s_webrtcConfig;

ServerMainWindow *wnd;
std::thread *messageThread;
rtc::Thread *rtcMainThread;

std::string s_server = "signalingserveruri";
uint32_t s_port = 3000;

bool s_closing = false;

typedef void(__stdcall*InputUpdateFuncType)(const char *msg);
typedef void(__stdcall*LogFuncType)(const int level, const char *msg);
typedef void(__stdcall*PeerConnectFuncType)(const int peerId, const char *peerName);
typedef void(__stdcall*PeerDisconnectFuncType)(const int peerId);

struct CallbackMap
{
	InputUpdateFuncType onInputUpdate;
	LogFuncType onLog;
	PeerConnectFuncType onPeerConnect;
	PeerDisconnectFuncType onPeerDisconnect;
} s_callbackMap;

#define ULOG(sev, msg) if (s_callbackMap.onLog) { (*s_callbackMap.onLog)(sev, msg); } LOG(sev) << msg


struct UnityServerPeerObserver : PeerConnectionClientObserver
{
	virtual void OnSignedIn() override
	{
		ULOG(INFO, __FUNCTION__);
		//TODO(bengreenier): we could surface this to unity
	}

	virtual void OnDisconnected() override
	{
		ULOG(INFO, __FUNCTION__);
		//TODO(bengreenier): we could surface this to unity
	}

	virtual void OnPeerConnected(int id, const std::string& name) override
	{
		if (s_callbackMap.onPeerConnect)
		{
			(*s_callbackMap.onPeerConnect)(id, name.c_str());
		}
	}

	virtual void OnPeerDisconnected(int peer_id) override
	{
		if (s_callbackMap.onPeerDisconnect)
		{
			(*s_callbackMap.onPeerDisconnect)(peer_id);
		}
	}

	virtual void OnMessageFromPeer(int peer_id, const std::string& message) override
	{
		//TODO(bengreenier): we could surface this to unity
	}

	virtual void OnMessageSent(int err) override
	{
		ULOG(INFO, __FUNCTION__);
		//TODO(bengreenier): we could surface this to unity
	}

	virtual void OnServerConnectionFailure() override
	{
		ULOG(INFO, __FUNCTION__);
		//TODO(bengreenier): we could surface this to unity
	}
} s_clientObserver;

void InitWebRTC()
{
	ULOG(INFO, __FUNCTION__);

	// Loads webrtc config file.
	ConfigParser::Parse("webrtcConfig.json", &s_webrtcConfig);

	ServerAuthenticationProvider::ServerAuthInfo authInfo;
	authInfo.authority = s_webrtcConfig.authentication.authority;
	authInfo.resource = s_webrtcConfig.authentication.resource;
	authInfo.clientId = s_webrtcConfig.authentication.client_id;
	authInfo.clientSecret = s_webrtcConfig.authentication.client_secret;

	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	rtc::InitializeSSL();

	rtcMainThread = &w32_thread;

	PeerConnectionClient client;
	std::shared_ptr<ServerAuthenticationProvider> authProvider;
	std::shared_ptr<TurnCredentialProvider> turnProvider;
	
	wnd = new ServerMainWindow(
		FLAG_server,
		FLAG_port,
		FLAG_autoconnect,
		FLAG_autocall,
		true,
		1280,
		720);

	wnd->Create();
	
	s_server = s_webrtcConfig.server;
	s_port = s_webrtcConfig.port;
	client.SetHeartbeatMs(s_webrtcConfig.heartbeat);

	s_conductor = new rtc::RefCountedObject<Conductor>(
		&client,
		wnd,
		&s_webrtcConfig,
		s_bufferRenderer);

	client.RegisterObserver(&s_clientObserver);

	InputDataHandler inputHandler([&](const std::string& message)
	{
		ULOG(INFO, __FUNCTION__);

		if (s_callbackMap.onInputUpdate)
		{
			ULOG(INFO, message.c_str());

			(*s_callbackMap.onInputUpdate)(message.c_str());
		}
	});

	s_conductor->SetInputDataHandler(&inputHandler);

	// configure callbacks (which may or may not be used)
	AuthenticationProvider::AuthenticationCompleteCallback authComplete([&](const AuthenticationProviderResult& data)
	{
		if (data.successFlag)
		{
			client.SetAuthorizationHeader("Bearer " + data.accessToken);

			// indicate to the user auth is complete and login (only if turn isn't in play)
			if (turnProvider.get() == nullptr)
			{
				wnd->SetAuthCode(L"OK");

				// login
				if (s_conductor != nullptr)
				{
					((MainWindowCallback*)s_conductor)->StartLogin(s_server, s_port);
				}
			}
		}
	});

	TurnCredentialProvider::CredentialsRetrievedCallback credentialsRetrieved([&](const TurnCredentials& creds)
	{
		if (creds.successFlag)
		{
			// indicate to the user turn is done
			wnd->SetAuthCode(L"OK");

			// login
			if (s_conductor != nullptr)
			{
				s_conductor->SetTurnCredentials(creds.username, creds.password);
			
				((MainWindowCallback*)s_conductor)->StartLogin(s_server, s_port);
			}
		}
	});

	// configure auth, if needed
	if (!authInfo.authority.empty())
	{
		authProvider.reset(new ServerAuthenticationProvider(authInfo));

		authProvider->SignalAuthenticationComplete.connect(&authComplete, &AuthenticationProvider::AuthenticationCompleteCallback::Handle);
	}

	// configure turn, if needed
	if (!s_webrtcConfig.turn_server.provider.empty())
	{
		turnProvider.reset(new TurnCredentialProvider(s_webrtcConfig.turn_server.provider));

		turnProvider->SignalCredentialsRetrieved.connect(&credentialsRetrieved, &TurnCredentialProvider::CredentialsRetrievedCallback::Handle);
	}

	// let the user know what we're doing
	if (turnProvider.get() != nullptr || authProvider.get() != nullptr)
	{
		if (authProvider.get() != nullptr)
		{
			wnd->SetAuthUri(std::wstring(authInfo.authority.begin(), authInfo.authority.end()));
		}

		wnd->SetAuthCode(L"Loading");
	}
	else
	{
		wnd->SetAuthCode(L"Not configured");
		wnd->SetAuthUri(L"Not configured");
	}

	// start auth or turn or login
	if (turnProvider.get() != nullptr)
	{
		if (authProvider.get() != nullptr)
		{
			turnProvider->SetAuthenticationProvider(authProvider.get());
		}

		// under the hood, this will trigger authProvider->Authenticate() if it exists
		turnProvider->RequestCredentials();
	}
	else if (authProvider.get() != nullptr)
	{
		authProvider->Authenticate();
	}
	else
	{
		// login
		((MainWindowCallback*)s_conductor)->StartLogin(s_server, s_port);
	}

	// Main loop.
	MSG msg;
	BOOL gm;
	while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1 && !s_closing)
	{
		if (!wnd->PreTranslateMessage(&msg))
		{
			try
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			catch (const std::exception& e) { // reference to the base of a polymorphic object
				std::cout << e.what(); // information from length_error printed
				ULOG(LERROR, e.what());
			}
		}
	}
}


static void UNITY_INTERFACE_API OnEncode(int eventID)
{
	if (s_Context)
	{
		if (s_frameBuffer == nullptr)
		{
			ID3D11RenderTargetView* rtv(nullptr);
			ID3D11DepthStencilView* depthStencilView(nullptr);

			s_Context->OMGetRenderTargets(1, &rtv, &depthStencilView);
			
			if (rtv)
			{
				rtv->GetResource(reinterpret_cast<ID3D11Resource**>(&s_frameBuffer));
				
				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
				rtv->GetDesc(&rtvDesc);

				rtv->Release();

				// Render loop.
				std::function<void()> frameRenderFunc = ([&]
				{
					// do nothing
				});

				// Initializes the buffer renderer.
				// TODO(bengreenier): we can optimize this sizing by reporting it to native-land from unity
				s_bufferRenderer = new BufferRenderer(
					1280 * 2,
					720,
					s_Device.Get(),
					frameRenderFunc,
					s_frameBuffer);

				s_frameBuffer->Release();

				messageThread = new std::thread(InitWebRTC);
			}
		}

		return;
	}
}

extern "C" void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	ULOG(INFO, __FUNCTION__);

	switch (eventType)
	{
		case kUnityGfxDeviceEventInitialize:
		{
			s_DeviceType = s_Graphics->GetRenderer();
			s_Device = s_UnityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice();
			s_Device->GetImmediateContext(&s_Context);

			break;
		}

		case kUnityGfxDeviceEventShutdown:
		{
			s_Context.Reset();
			s_Device.Reset();
			s_DeviceType = kUnityGfxRendererNull;

			break;
		}
	}
}



extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	ULOG(INFO, __FUNCTION__);

#if SHOW_CONSOLE
	AllocConsole();
	FILE* out(nullptr);
	freopen_s(&out, "CONOUT$", "w", stdout);

	std::cout << "Console open..." << std::endl;
	ULOG(INFO, "Console open...")
#endif

	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" __declspec(dllexport) void Close()
{
	ULOG(INFO, __FUNCTION__);

	if (s_conductor != nullptr)
	{
		MainWindowCallback *callback = s_conductor;
		callback->DisconnectFromCurrentPeer();
		callback->DisconnectFromServer();

		callback->Close();

		s_conductor = nullptr;

		rtc::CleanupSSL();

		s_closing = true;
	}
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	ULOG(INFO, __FUNCTION__);

	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);

	Close();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	ULOG(INFO, __FUNCTION__);

	return OnEncode;
}

extern "C" __declspec(dllexport) void SetCallbackMap(InputUpdateFuncType inputFunc,
	LogFuncType logFunc,
	PeerConnectFuncType peerConnectFunc,
	PeerDisconnectFuncType peerDisconnectFunc)
{
	ULOG(INFO, __FUNCTION__);
	
	s_callbackMap.onInputUpdate = inputFunc;
	s_callbackMap.onLog = logFunc;
	s_callbackMap.onPeerConnect = peerConnectFunc;
	s_callbackMap.onPeerDisconnect = peerDisconnectFunc;
}

extern "C" __declspec(dllexport) void ConnectToPeer(const int peerId)
{
	ULOG(INFO, __FUNCTION__);

	// TODO(bengreenier): *maniacal laughter* does this work?
	rtcMainThread->Invoke<void>(RTC_FROM_HERE, [&] {
		s_conductor->ConnectToPeer(peerId);
	});
}

extern "C" __declspec(dllexport) void DisconnectFromPeer()
{
	ULOG(INFO, __FUNCTION__);

	// TODO(bengreenier): *maniacal laughter* does this work?
	rtcMainThread->Invoke<void>(RTC_FROM_HERE, [&] {
		s_conductor->DisconnectFromCurrentPeer();
	});
}