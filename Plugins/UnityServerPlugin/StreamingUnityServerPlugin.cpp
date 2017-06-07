#define SUPPORT_D3D11 1
#define WEBRTC_WIN 1

#define SHOW_CONSOLE 1


#include <iostream>
#include <thread>
#include <string>
#include <cstdint>
#include <wrl.h>
#include <d3d11_2.h>
#include "IUnityGraphicsD3D11.h"
#include "IUnityGraphics.h"
#include "IUnityInterface.h"

#include "video_helper.h"
#include "conductor.h"
#include "default_main_window.h"
#include "flagdefs.h"
#include "peer_connection_client.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"

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
using namespace Toolkit3DLibrary;

void(__stdcall*s_onInputUpdate)(const char *msg);

DEFINE_GUID(IID_Texture2D, 0x6f15aaf2, 0xd208, 0x4e89, 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c);


static IUnityInterfaces* s_UnityInterfaces = nullptr;
static IUnityGraphics* s_Graphics = nullptr;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static ComPtr<ID3D11Device> s_Device;
static ComPtr<ID3D11DeviceContext> s_Context;

static rtc::scoped_refptr<Conductor> s_conductor = nullptr;

VideoHelper*				g_videoHelper = nullptr;
static ID3D11Texture2D*		s_frameBuffer = nullptr;
static ID3D11Texture2D*		s_frameBufferCopy = nullptr;


DefaultMainWindow *wnd;
std::thread *messageThread;

std::string s_server = "signalingserver.centralus.cloudapp.azure.com";
uint32_t s_port = 3000;


extern "C" __declspec(dllexport) void MsgBox(char *msg)
{
#if SHOW_CONSOLE
	std::cout << msg << std::endl;
#endif
	//MessageBoxA(NULL, msg, "", MB_OK);
}

void FrameUpdate()
{
}

// Handles input from client.
void InputUpdate(const std::string& message)
{
	if (s_onInputUpdate)
	{
		LOG(INFO) << message;

		(*s_onInputUpdate)(message.c_str());
	}
}


void InitWebRTC()
{
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
	rtc::InitializeSSL();
	
	PeerConnectionClient client;

	wnd = new DefaultMainWindow(FLAG_server, FLAG_port, FLAG_autoconnect, FLAG_autocall,
		true, 1280, 720);
	
	wnd->Create();

	s_conductor = new rtc::RefCountedObject<Conductor>(&client, wnd, &FrameUpdate, &InputUpdate, g_videoHelper);
	
	if (s_conductor != nullptr)
	{
		MainWindowCallback *callback = s_conductor;

		callback->StartLogin(s_server, s_port);
	}

	// Main loop.
	MSG msg;
	BOOL gm;
	while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
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
			}
		}
	}

	int aaa = 3;
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
				rtv->Release();

				D3D11_TEXTURE2D_DESC desc;

				s_frameBuffer->GetDesc(&desc);
					
				g_videoHelper->Initialize(s_frameBuffer, desc.Format, desc.Width, desc.Height);
				
				s_frameBuffer->Release();
				
				messageThread = new std::thread(InitWebRTC);
			}
		}

        return;
    }
}

extern "C" void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
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
			MsgBox("kUnityGfxDeviceEventShutdown enter");

            s_Context.Reset();
            s_Device.Reset();
            s_DeviceType = kUnityGfxRendererNull;

			MsgBox("kUnityGfxDeviceEventShutdown exit");

            break;
        }
    }
}



extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
#if SHOW_CONSOLE
    AllocConsole();
    FILE* out(nullptr);
    freopen_s(&out, "CONOUT$", "w", stdout);

    std::cout << "Console open..." << std::endl;
#endif
	
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);

	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(s_Device.Get(), s_Context.Get());
}

extern "C" __declspec(dllexport) void Close()
{
	if (s_conductor != nullptr)
	{
		MsgBox("Close enter");

		MainWindowCallback *callback = s_conductor;
		callback->DisconnectFromCurrentPeer(); 
		callback->DisconnectFromServer();
		
		callback->Close();

		s_conductor = nullptr;
		MsgBox("Close exit");

		rtc::CleanupSSL();
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	MsgBox("UnityPluginUnload enter");

	Close();

    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	MsgBox("UnityPluginUnload exit");
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
    return OnEncode;
}

extern "C" __declspec(dllexport) void Login(char *server, int32_t port)
{
	s_server = server;
	s_port = port;

	s_onInputUpdate = nullptr;

	if (s_conductor != nullptr)
	{
		MainWindowCallback *callback = s_conductor;

		callback->StartLogin(s_server, s_port);
	}
}



extern "C" __declspec(dllexport) void SetInputDataCallback(void(__stdcall*onInputUpdate)(const char *msg))
{
	s_onInputUpdate = onInputUpdate;
}

