#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "DeviceResources.h"
#include "CubeRenderer.h"
#include "macros.h"

#ifdef TEST_RUNNER
#include "test_runner.h"
#else // TEST_RUNNER
#include "server_main_window.h"
#include "server_authentication_provider.h"
#include "turn_credential_provider.h"
#include "server_renderer.h"
#include "webrtc.h"
#include "config_parser.h"
#include "directx_buffer_capturer.h"
#include "service/render_service.h"
#include "multi_peer_conductor.hpp"
#endif // TEST_RUNNER

#define FOCUS_POINT		3.f

// Required app libs
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

#ifndef TEST_RUNNER
using namespace Microsoft::WRL;
using namespace Windows::Foundation::Numerics;
#endif // TEST_RUNNER

using namespace DX;
using namespace StreamingToolkit;
using namespace StreamingToolkitSample;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
void StartRenderService();

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HWND					g_hWnd = nullptr;
DeviceResources*		g_deviceResources = nullptr;
CubeRenderer*			g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
VideoTestRunner*		g_videoTestRunner = nullptr;
#else // TEST_RUNNER
bool					g_hasNewInputData = false;
int64_t					g_lastTimestamp = -1;
DirectX::XMVECTORF32	g_lookAtVector;
DirectX::XMVECTORF32	g_upVector;
DirectX::XMVECTORF32	g_eyeVector;
DirectX::XMFLOAT4X4		g_viewProjectionMatrixLeft;
DirectX::XMFLOAT4X4		g_viewProjectionMatrixRight;
#endif // TESTRUNNER

#ifndef TEST_RUNNER

bool AppMain(BOOL stopping)
{
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();
	auto serverConfig = GlobalObject<ServerConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();
	
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

	ServerMainWindow wnd(
		webrtcConfig->server.c_str(),
		webrtcConfig->port,
		FLAG_autoconnect,
		FLAG_autocall,
		false,
		serverConfig->server_config.width,
		serverConfig->server_config.height);

	// give us a quick and dirty quit handler
	struct wndHandler : public MainWindowCallback
	{
		virtual void StartLogin(const std::string& server, int port) override {};

		virtual void DisconnectFromServer() override {}

		virtual void ConnectToPeer(int peer_id) override {}

		virtual void DisconnectFromCurrentPeer() override {}

		virtual void UIThreadCallback(int msg_id, void* data) override {}

		atomic_bool isClosing = false;
		virtual void Close() override { isClosing.store(true); }
	} wndHandler;
	
	// register the handler
	wnd.RegisterObserver(&wndHandler);

	if (!serverConfig->server_config.system_service && !wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(wnd.handle());

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	rtc::InitializeSSL();
	std::shared_ptr<ServerAuthenticationProvider> authProvider;
	std::shared_ptr<TurnCredentialProvider> turnProvider;
	PeerConnectionClient client;

	// Creates and initializes the buffer capturer.
	// Note: Conductor is responsible for cleaning up bufferCapturer object.
	std::shared_ptr<DirectXBufferCapturer> bufferCapturer = std::shared_ptr<DirectXBufferCapturer>(
		new DirectXBufferCapturer(g_deviceResources->GetD3DDevice()));

	bufferCapturer->Initialize(serverConfig->server_config.system_service,
		serverConfig->server_config.width, serverConfig->server_config.height);

	if (nvEncConfig->use_software_encoding)
	{
		bufferCapturer->EnableSoftwareEncoder();
	}

	// Initializes the conductor.
	rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(
		&client, bufferCapturer.get(), &wnd, webrtcConfig.get()));

	// Gets the frame buffer from the swap chain.
	ComPtr<ID3D11Texture2D> frameBuffer;
	if (!serverConfig->server_config.system_service)
	{
		HRESULT hr = g_deviceResources->GetSwapChain()->GetBuffer(
			0,
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(frameBuffer.GetAddressOf()));
	}

	// Initializes the conductor.
	MultiPeerConductor cond(webrtcConfig,
		g_deviceResources->GetD3DDevice(),
		nvEncConfig->use_software_encoding);

	cond.ConnectSignallingAsync("renderingserver_test");

	// Handles input from client.
	InputDataHandler inputHandler([&](const std::string& message)
	{
		// if we're quitting, do so
		if (wndHandler.isClosing)
		{
			break;
		}

		MSG msg = { 0 };

		if (msg.isMember("type") && msg.isMember("body"))
		{
			strcpy(type, msg.get("type", "").asCString());
			strcpy(body, msg.get("body", "").asCString());
			std::istringstream datastream(body);
			std::string token;
			if (strcmp(type, "stereo-rendering") == 0)
			{
				if (!wnd.PreTranslateMessage(&msg))
				{
					ID3D11Texture2D* frameBuffer = nullptr;
					HRESULT hr = g_deviceResources->GetSwapChain()->GetBuffer(
						0,
						__uuidof(ID3D11Texture2D),
						reinterpret_cast<void**>(&frameBuffer));

					g_bufferRenderer->Resize(frameBuffer);

					if (isStereo)
					{
						// In stereo rendering mode, we need to position the cube
						// in front of user.
						g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, FOCUS_POINT }));
					}
					else
					{
						g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, 0.f }));
					}
				}
			}
			else if (strcmp(type, "camera-transform-lookat") == 0)
			{
				ULONGLONG tick = GetTickCount64();
				g_cubeRenderer->Update();

				for each (auto pair in cond.Peers())
				{
					auto peer = pair.second;
					auto peerView = peer->View();

					if (peerView->IsValid())
					{
						g_cubeRenderer->UpdateView(peerView->eye, peerView->lookAt, peerView->up);
					}

					g_cubeRenderer->Render();

					// this works because peer is a DirectXPeerConductor
					peer->SendFrame(frameBuffer.Get());
				}

				// TODO(bengreenier): this will only show the last viewport via the server window (is that cool)
				g_deviceResources->Present();

				// FPS limiter.
				const int interval = 1000 / nvEncConfig->capture_fps;
				ULONGLONG timeElapsed = GetTickCount64() - tick;
				DWORD sleepAmount = 0;
				if (timeElapsed < interval)
				{
					sleepAmount = interval - timeElapsed;
				}

				Sleep(sleepAmount);
			}
		}
	});

	for (auto i = 0; i < 2; ++i)
	{
		threads.push_back(new std::thread([&, i]()
		{
			rtc::Win32Thread instanceThread;
			instanceThread.SetName("Instance[" + std::to_string(i) + "]", nullptr);

			rtc::ThreadManager::Instance()->SetCurrentThread(&instanceThread);

			PeerConnectionClient client("[" + std::to_string(i) + "]");
			rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(
				&client, &wnd, webrtcConfig.get(), g_bufferRenderer));

			conductor->SetInputDataHandler(&inputHandler);
			client.SetHeartbeatMs(webrtcConfig->heartbeat);

			while (!stopThreads)
			{
				instanceThread.ProcessMessages(500);
			}

			/*
			MSG msg;
			BOOL gm;
			while (!stopping && (gm = ::GetMessage(&msg, (HWND)-1, 0, 0)) != 0 && gm != -1)
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}*/
			
			// Main loop.
			//MSG msg;
			//BOOL gm;
			//while (!stopping && (gm = ::GetMessage(&msg, (HWND)-1, 0, 0)) != 0 && gm != -1)
			//{
			//	// For system service, ignore window and swap chain.
			//	if (serverConfig->server_config.system_service)
			//	{
			//		::TranslateMessage(&msg);
			//		::DispatchMessage(&msg);
			//	}
			//	else
			//	{
			//		if (!wnd.PreTranslateMessage(&msg))
			//		{
			//			::TranslateMessage(&msg);
			//			::DispatchMessage(&msg);
			//		}
			//	}
			//}
		}));
	}

	rtc::CleanupSSL();

	// Cleanup.
	delete g_cubeRenderer;
	delete g_deviceResources;

	return 0;
}

//--------------------------------------------------------------------------------------
// System service
//--------------------------------------------------------------------------------------
void StartRenderService()
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager)
	{
		// Init service's main function.
		const std::function<void(BOOL*)> serviceMainFunc = [&](BOOL* stopping)
		{
			AppMain(*stopping);
		};

		auto serverConfig = GlobalObject<ServerConfig>::Get();

		RenderService service((PWSTR)serverConfig->service_config.name.c_str(), serviceMainFunc);

		// Starts the service to run the app persistently.
		if (!CServiceBase::Run(service))
		{
			wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
			MessageBox(
				NULL,
				L"Service needs to be initialized using PowerShell scripts.",
				L"Error",
				MB_ICONERROR
			);
		}

		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
}

#else // TEST_RUNNER

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// Note that this tutorial does not handle resizing (WM_SIZE) requests,
		// so we created the window without the resize border.

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//--------------------------------------------------------------------------------------
// Registers class and creates window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Registers class.
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"SpinningCubeClass";
	if (!RegisterClassEx(&wcex))
	{
		return E_FAIL;
	}

	// Creates window.
	RECT rc = { 0, 0, 1280, 720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(
		L"SpinningCubeClass",
		L"SpinningCube",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (!g_hWnd)
	{
		return E_FAIL;
	}

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the frame
//--------------------------------------------------------------------------------------
void Render()
{
	g_cubeRenderer->Update();
	g_cubeRenderer->Render();
	g_deviceResources->Present();
}

#endif // TEST_RUNNER

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef TEST_RUNNER
	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return 0;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(g_hWnd);

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	// Creates and initializes the video test runner library.
	g_videoTestRunner = new VideoTestRunner(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

	g_videoTestRunner->StartTestRunner(g_deviceResources->GetSwapChain());

	// Main message loop.
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();

			if (g_videoTestRunner->TestsComplete())
			{
				break;
			}

			g_videoTestRunner->TestCapture();
			if (g_videoTestRunner->IsNewTest())
			{
				delete g_cubeRenderer;
				g_cubeRenderer = new CubeRenderer(g_deviceResources);
			}
		}
	}

	delete g_cubeRenderer;
	delete g_deviceResources;

	return (int)msg.wParam;
#else // TEST_RUNNER

	// setup the config parsers
	ConfigParser::ConfigureConfigFactories();

	auto serverConfig = GlobalObject<ServerConfig>::Get();

	if (!serverConfig->server_config.system_service)
	{
		return AppMain(FALSE);
	}
	else
	{
		StartRenderService();
		return 0;
	}
#endif // TEST_RUNNER
}