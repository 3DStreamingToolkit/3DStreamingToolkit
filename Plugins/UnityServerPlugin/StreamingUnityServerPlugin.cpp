#define SUPPORT_D3D11		1
#define WEBRTC_WIN			1

#define SHOW_CONSOLE 0
#define ULOG(sev, msg)						\
	if (s_callbackMap.onLog)				\
	{										\
		(*s_callbackMap.onLog)(sev, msg);	\
	}										\
											\
	LOG(sev) << msg							

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

#include "config_parser.h"
#include "flagdefs.h"
#include "directx_multi_peer_conductor.h"
#include "server_main_window.h"

#include "webrtc/modules/video_coding/codecs/h264/h264_encoder_impl.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"
#include "webrtc/base/logging.h"

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

static IUnityInterfaces*			s_UnityInterfaces		= nullptr;
static IUnityGraphics*				s_Graphics				= nullptr;
static UnityGfxRenderer				s_DeviceType			= kUnityGfxRendererNull;
static ComPtr<ID3D11Device>			s_Device;
static ComPtr<ID3D11DeviceContext>	s_Context;

static std::thread*					s_messageThread;
static rtc::Thread*					s_rtcMainThread;

static std::string					s_server				= "signalingserveruri";
static uint32_t						s_port					= 3000;
static bool							s_closing				= false;
static std::shared_ptr<DirectXMultiPeerConductor> s_cond;

typedef void(__stdcall*NoParamFuncType)();
typedef void(__stdcall*IntParamFuncType)(const int val);
typedef void(__stdcall*BoolParamFuncType)(const bool val);
typedef void(__stdcall*StrParamFuncType)(const char* str);
typedef void(__stdcall*IntStringParamsFuncType)(const int val, const char* str);

struct CallbackMap
{
	IntStringParamsFuncType onDataChannelMessage;
	IntStringParamsFuncType onLog;
	IntStringParamsFuncType onPeerConnect;
	IntParamFuncType onPeerDisconnect;
	NoParamFuncType onSignIn;
	NoParamFuncType onDisconnect;
	IntStringParamsFuncType onMessageFromPeer;
	IntParamFuncType onMessageSent;
	NoParamFuncType onServerConnectionFailure;
	IntParamFuncType onSignalingChange;
	StrParamFuncType onAddStream;
	StrParamFuncType onRemoveStream;
	StrParamFuncType onDataChannel;
	NoParamFuncType onRenegotiationNeeded;
	IntParamFuncType onIceConnectionChange;
	IntParamFuncType onIceGatheringChange;
	StrParamFuncType onIceCandidate;
	BoolParamFuncType onIceConnectionReceivingChange;
} s_callbackMap;

struct UnityServerPeerObserver : public PeerConnectionClientObserver,
	public webrtc::PeerConnectionObserver
{
	virtual void OnSignedIn() override
	{
		if (s_callbackMap.onSignIn)
		{
			(*s_callbackMap.onSignIn)();
		}
	}

	virtual void OnDisconnected() override
	{
		if (s_callbackMap.onDisconnect)
		{
			(*s_callbackMap.onDisconnect)();
		}
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
		if (s_callbackMap.onMessageFromPeer)
		{
			(*s_callbackMap.onMessageFromPeer)(peer_id, message.c_str());
		}
	}

	virtual void OnMessageSent(int err) override
	{
		if (s_callbackMap.onMessageSent)
		{
			(*s_callbackMap.onMessageSent)(err);
		}
	}

	virtual void OnServerConnectionFailure() override
	{
		if (s_callbackMap.onServerConnectionFailure)
		{
			(*s_callbackMap.onServerConnectionFailure)();
		}
	}

	virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override
	{
		if (s_callbackMap.onSignalingChange)
		{
			(*s_callbackMap.onSignalingChange)(new_state);
		}
	}

	virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override
	{
		if (s_callbackMap.onAddStream)
		{
			(*s_callbackMap.onAddStream)(stream->label().c_str());
		}
	}

	virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override
	{
		if (s_callbackMap.onRemoveStream)
		{
			(*s_callbackMap.onRemoveStream)(stream->label().c_str());
		}
	}

	virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override
	{
		if (s_callbackMap.onDataChannel)
		{
			(*s_callbackMap.onDataChannel)(channel->label().c_str());
		}
	}

	virtual void OnRenegotiationNeeded() override
	{
		if (s_callbackMap.onRenegotiationNeeded)
		{
			(*s_callbackMap.onRenegotiationNeeded)();
		}
	}

	virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override
	{
		if (s_callbackMap.onIceConnectionChange)
		{
			(*s_callbackMap.onIceConnectionChange)(new_state);
		}
	}

	virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override
	{
		if (s_callbackMap.onIceGatheringChange)
		{
			(*s_callbackMap.onIceGatheringChange)(new_state);
		}
	}

	virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override
	{
		if (s_callbackMap.onIceCandidate)
		{
			std::string sdp;

			if (candidate->ToString(&sdp))
			{
				(*s_callbackMap.onIceCandidate)(sdp.c_str());
			}
		}
	}

	virtual void OnIceConnectionReceivingChange(bool receiving) override
	{
		if (s_callbackMap.onIceConnectionReceivingChange)
		{
			(*s_callbackMap.onIceConnectionReceivingChange)(receiving);
		}
	}

} s_clientObserver;

void InitWebRTC()
{
	ULOG(INFO, __FUNCTION__);

	// Setup the config parsers.
	ConfigParser::ConfigureConfigFactories();

	auto fullServerConfig = GlobalObject<FullServerConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	s_rtcMainThread = &w32_thread;

	ServerMainWindow wnd(
		FLAG_server,
		FLAG_port,
		fullServerConfig->server_config->server_config.auto_connect,
		fullServerConfig->server_config->server_config.auto_call,
		true,
		0,
		0);

	wnd.Create();

	// Initializes SSL.
	rtc::InitializeSSL();
		
	s_server = fullServerConfig->webrtc_config->server;
	s_port = fullServerConfig->webrtc_config->port;

	// Initializes the conductor.
	s_cond.reset(new DirectXMultiPeerConductor(fullServerConfig, s_Device.Get()));

	// Registers observer to update Unity's window UI.
	s_cond->PeerConnection().RegisterObserver(&s_clientObserver);

	// Handles data channel messages.
	std::function<void(int, const string&)> dataChannelMessageHandler([&](
		int peerId,
		const std::string& message)
	{
		ULOG(INFO, message.c_str());

		if (s_callbackMap.onDataChannelMessage)
		{
			(*s_callbackMap.onDataChannelMessage)(peerId, message.c_str());
		}
	});

	s_cond->SetDataChannelMessageHandler(dataChannelMessageHandler);

	s_cond->StartLogin(s_server, s_port);

	// Main loop.
	MSG msg;
	BOOL gm;
	while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1 && !s_closing && WM_QUIT != msg.message)
	{
		if (!wnd.PreTranslateMessage(&msg))
		{
			try
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			catch (const std::exception& e) 
			{
				// reference to the base of a polymorphic object
				std::cout << e.what(); // information from length_error printed
				ULOG(LERROR, e.what());
			}
		}
	}
}

extern "C" void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Note: we can't call any marshalled stuff from this hook.

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

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load.
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" __declspec(dllexport) void Close()
{
	ULOG(INFO, __FUNCTION__);

	s_cond->DisconnectFromCurrentPeer();
	s_cond->DisconnectFromServer();
	s_cond->Close();
	rtc::CleanupSSL();
	s_closing = true;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);

	Close();
}

extern "C" __declspec(dllexport) void NativeInitWebRTC()
{
	s_messageThread = new std::thread(InitWebRTC);
}

extern "C" __declspec(dllexport) void ConnectToPeer(const int peerId)
{
	ULOG(INFO, __FUNCTION__);

	// Marshal to main thread.
	s_rtcMainThread->Invoke<void>(RTC_FROM_HERE, [&] {
		s_cond->ConnectToPeer(peerId);
	});
}

extern "C" __declspec(dllexport) void SendFrame(int peerId, bool isStereo, void* leftRT, void* rightRT, int64_t predictionTimestamp)
{
	auto it = s_cond->Peers().find(peerId);
	if (it != s_cond->Peers().end())
	{
		DirectXPeerConductor* peer = it->second.get();
		if (!isStereo)
		{
			peer->SendFrame((ID3D11Texture2D*)leftRT);
		}
		else
		{
			peer->SendFrame((ID3D11Texture2D*)leftRT, (ID3D11Texture2D*)rightRT, predictionTimestamp);
		}
	}
}

extern "C" __declspec(dllexport) void SetCallbackMap(IntStringParamsFuncType onDataChannelMessage,
	IntStringParamsFuncType onLog,
	IntStringParamsFuncType onPeerConnect,
	IntParamFuncType onPeerDisconnect,
	NoParamFuncType onSignIn,
	NoParamFuncType onDisconnect,
	IntStringParamsFuncType onMessageFromPeer,
	IntParamFuncType onMessageSent,
	NoParamFuncType onServerConnectionFailure,
	IntParamFuncType onSignalingChange,
	StrParamFuncType onAddStream,
	StrParamFuncType onRemoveStream,
	StrParamFuncType onDataChannel,
	NoParamFuncType onRenegotiationNeeded,
	IntParamFuncType onIceConnectionChange,
	IntParamFuncType onIceGatheringChange,
	StrParamFuncType onIceCandidate,
	BoolParamFuncType onIceConnectionReceivingChange)
{
	s_callbackMap.onDataChannelMessage = onDataChannelMessage;
	s_callbackMap.onLog = onLog;
	s_callbackMap.onPeerConnect = onPeerConnect;
	s_callbackMap.onPeerDisconnect = onPeerDisconnect;
	s_callbackMap.onSignIn = onSignIn;
	s_callbackMap.onDisconnect = onDisconnect;
	s_callbackMap.onMessageFromPeer = onMessageFromPeer;
	s_callbackMap.onMessageSent = onMessageSent;
	s_callbackMap.onServerConnectionFailure = onServerConnectionFailure;
	s_callbackMap.onSignalingChange = onSignalingChange;
	s_callbackMap.onAddStream = onAddStream;
	s_callbackMap.onRemoveStream = onRemoveStream;
	s_callbackMap.onDataChannel = onDataChannel;
	s_callbackMap.onRenegotiationNeeded = onRenegotiationNeeded;
	s_callbackMap.onIceConnectionChange = onIceConnectionChange;
	s_callbackMap.onIceGatheringChange = onIceGatheringChange;
	s_callbackMap.onIceCandidate = onIceCandidate;
	s_callbackMap.onIceConnectionReceivingChange = onIceConnectionReceivingChange;
}
