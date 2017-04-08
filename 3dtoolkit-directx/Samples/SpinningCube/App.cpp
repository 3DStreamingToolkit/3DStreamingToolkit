#include "pch.h"
#include "DeviceResources.h"
#include "CubeRenderer.h"

#ifdef TEST_RUNNER
#include "VideoTestRunner.h"
#else // TEST_RUNNER
#include "VideoHelper.h"
#endif // TEST_RUNNER

#ifdef REMOTE_RENDERING
#include "conductor.h"
#include "default_main_window.h"
#include "flagdefs.h"
#include "peer_connection_client.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"
#endif // REMOTE_RENDERING

using namespace DX;
using namespace Toolkit3DLibrary;
using namespace Toolkit3DSample;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HWND				g_hWnd = nullptr;
DeviceResources*	g_deviceResources = nullptr;
CubeRenderer*		g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
VideoTestRunner*	g_videoTestRunner = nullptr;
#else // TEST_RUNNER
VideoHelper*		g_videoHelper = nullptr;
#endif // TESTRUNNER


//--------------------------------------------------------------------------------------
// Global Methods
//--------------------------------------------------------------------------------------
void FrameUpdate()
{
	g_cubeRenderer->Update();
	g_cubeRenderer->Render();
}

#ifdef REMOTE_RENDERING

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------
int InitWebRTC()
{
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

#ifdef SERVER_APP
	DefaultMainWindow wnd(FLAG_server, FLAG_port, FLAG_autoconnect, FLAG_autocall,
		true, 1280, 720);
#else // SERVER_APP
	DefaultMainWindow wnd(FLAG_server, FLAG_port, FLAG_autoconnect, FLAG_autocall,
		false, 1280, 720);
#endif // SERVER_APP

	if (!wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(wnd.handle());

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

#ifdef WEBRTC_RAW_ENCODED_FRAME
	g_videoHelper->Initialize(g_deviceResources->GetSwapChain(), nullptr, true);
#else // WEBRTC_RAW_ENCODED_FRAME
	g_videoHelper->Initialize(g_deviceResources->GetSwapChain());
#endif // WEBRTC_RAW_ENCODED_FRAME

	rtc::InitializeSSL();
	PeerConnectionClient client;

#ifdef SERVER_APP
	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(
			&client, &wnd, &FrameUpdate, nullptr, g_videoHelper, true));
#else // SERVER_APP
	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(
			&client, &wnd, &FrameUpdate, nullptr, g_videoHelper, false));
#endif // SERVER_APP

	// Main loop.
	MSG msg;
	BOOL gm;
	while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
	{
		if (!wnd.PreTranslateMessage(&msg))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	if (conductor->connection_active() || client.is_connected())
	{
		while ((conductor->connection_active() || client.is_connected()) &&
			(gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
		{
			if (!wnd.PreTranslateMessage(&msg))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
	}

	rtc::CleanupSSL();

	// Cleanup.
	delete g_videoHelper;
	delete g_cubeRenderer;
	delete g_deviceResources;

	return 0;
}

#else // REMOTE_RENDERING

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
#ifdef STEREO_OUTPUT_MODE
	RECT rc = { 0, 0, 2560, 720 };
#else
	RECT rc = { 0, 0, 1280, 720 };
#endif
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
	FrameUpdate();
	g_deviceResources->Present();
}

#endif // REMOTE_RENDERING

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

#ifndef REMOTE_RENDERING
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

#ifdef TEST_RUNNER
	// Creates and initializes the video test runner library.
	g_videoTestRunner = new VideoTestRunner(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext()); 

	g_videoTestRunner->StartTestRunner(g_deviceResources->GetSwapChain());
#else // TEST_RUNNER
	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

	g_videoHelper->Initialize(g_deviceResources->GetSwapChain(), "output.h264");
#endif // TEST_RUNNER
	
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
#ifdef TEST_RUNNER
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
#endif // TEST_RUNNER
		}
	}

	delete g_cubeRenderer;
	delete g_deviceResources;

	return (int)msg.wParam;
#else // REMOTE_RENDERING
	return InitWebRTC();
#endif // REMOTE_RENDERING
}

