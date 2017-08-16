#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "DeviceResources.h"
#include "CubeRenderer.h"
#include "server_authentication_provider.h"
#include "turn_credential_provider.h"

#ifdef TEST_RUNNER
#include "test_runner.h"
#else // TEST_RUNNER
#include "server_renderer.h"
#include "webrtc.h"
#endif // TEST_RUNNER

// Required app libs
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

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

#ifndef TEST_RUNNER

// Handles input from client.
void InputUpdate(const std::string& message)
{
	char type[256];
	char body[1024];
	Json::Reader reader;
	Json::Value msg = NULL;
	reader.parse(message, msg, false);

	if (msg.isMember("type") && msg.isMember("body"))
	{
		strcpy(type, msg.get("type", "").asCString());
		strcpy(body, msg.get("body", "").asCString());
		std::istringstream datastream(body);
		std::string token;

		if (strcmp(type, "stereo-rendering") == 0)
		{
			getline(datastream, token, ',');
			int stereo = stoi(token);
			g_deviceResources->SetStereo(stereo == 1);
		}
		else if (strcmp(type, "camera-transform-lookat") == 0)
		{			
			// Eye point.
			getline(datastream, token, ',');
			float eyeX = stof(token);
			getline(datastream, token, ',');
			float eyeY = stof(token);
			getline(datastream, token, ',');
			float eyeZ = stof(token);

			// Focus point.
			getline(datastream, token, ',');
			float focusX = stof(token);
			getline(datastream, token, ',');
			float focusY = stof(token);
			getline(datastream, token, ',');
			float focusZ = stof(token);

			// Up vector.
			getline(datastream, token, ',');
			float upX = stof(token);
			getline(datastream, token, ',');
			float upY = stof(token);
			getline(datastream, token, ',');
			float upZ = stof(token);

			const DirectX::XMVECTORF32 lookAt = { focusX, focusY, focusZ, 0.f };
			const DirectX::XMVECTORF32 up = { upX, upY, upZ, 0.f };
			const DirectX::XMVECTORF32 eye = { eyeX, eyeY, eyeZ, 0.f };
			g_cubeRenderer->UpdateView(eye, lookAt, up);
		}
		else if (strcmp(type, "camera-transform-stereo") == 0)
		{
			// Parses the left view projection matrix.
			DirectX::XMFLOAT4X4 viewProjectionLeft;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					getline(datastream, token, ',');
					viewProjectionLeft.m[i][j] = stof(token);
				}
			}

			// Parses the right view projection matrix.
			DirectX::XMFLOAT4X4 viewProjectionRight;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					getline(datastream, token, ',');
					viewProjectionRight.m[i][j] = stof(token);
				}
			}

			// Updates the cube's matrices.
			g_cubeRenderer->UpdateView(
				viewProjectionLeft, viewProjectionRight);
		}
	}
}

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------
int InitWebRTC(char* server, int port, int heartbeat,
	const ServerAuthenticationProvider::ServerAuthInfo& authInfo,
	const std::string& turnCredentialUri)
{
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

#ifdef NO_UI
	DefaultMainWindow wnd(server, port, true, true, true);
#else // NO_UI
	DefaultMainWindow wnd(server, port, FLAG_autoconnect, FLAG_autocall, false, 1280, 720);
#endif // NO_UI

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

	g_videoHelper->Initialize(g_deviceResources->GetSwapChain());

	rtc::InitializeSSL();

	std::shared_ptr<ServerAuthenticationProvider> authProvider;
	std::shared_ptr<TurnCredentialProvider> turnProvider;
	PeerConnectionClient client;

	if (!authInfo.authority.empty())
	{
		authProvider.reset(new ServerAuthenticationProvider(authInfo));

		AuthenticationProvider::AuthenticationCompleteCallback authComplete([&](const AuthenticationProviderResult& data) {
			if (data.successFlag)
			{
				client.SetAuthorizationHeader("Bearer " + data.accessToken);
			}
		});

		authProvider->SignalAuthenticationComplete.connect(&authComplete, &AuthenticationProvider::AuthenticationCompleteCallback::Handle);
	}

	client.SetHeartbeatMs(heartbeat);

	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(
			&client, &wnd, &FrameUpdate, &InputUpdate, g_videoHelper));


	if (!turnCredentialUri.empty())
	{
		turnProvider.reset(new TurnCredentialProvider(turnCredentialUri));

		TurnCredentialProvider::CredentialsRetrievedCallback credentialsRetrieved([&](const TurnCredentials& creds)
		{
			if (creds.successFlag)
			{
				conductor->SetTurnCredentials(creds.username, creds.password);
			}
		});

		turnProvider->SignalCredentialsRetrieved.connect(&credentialsRetrieved, &TurnCredentialProvider::CredentialsRetrievedCallback::Handle);

		if (authProvider.get() != nullptr)
		{
			// set an auth provider, upon authenticating it will trigger turn credential retrieval automatically
			turnProvider->SetAuthenticationProvider(authProvider.get());

			// if we have auth, first get it
			authProvider->Authenticate();
		}
		else
		{
			// no auth, just get creds
			turnProvider->RequestCredentials();
		}
	}

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

		if (conductor->connection_active() || client.is_connected())
		{
			g_deviceResources->Present();
		}
	}

	rtc::CleanupSSL();

	// Cleanup.
	delete g_videoHelper;
	delete g_cubeRenderer;
	delete g_deviceResources;

	return 0;
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
	FrameUpdate();
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
	int nArgs;
	char server[1024];
	strcpy(server, FLAG_server);
	int port = FLAG_port;
	int heartbeat = FLAG_heartbeat;
	std::string turnCredentialUri;
	ServerAuthenticationProvider::ServerAuthInfo authInfo;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

	// Try parsing command line arguments.
	if (szArglist && nArgs == 2)
	{
		wcstombs(server, szArglist[0], sizeof(server));
		port = _wtoi(szArglist[1]);
	}
	else // Try parsing config file.
	{
		std::string configFilePath = ExePath("webrtcConfig.json");
		std::ifstream configFile(configFilePath);
		Json::Reader reader;
		Json::Value root = NULL;
		if (configFile.good())
		{
			reader.parse(configFile, root, true);
			if (root.isMember("server"))
			{
				strcpy(server, root.get("server", FLAG_server).asCString());
			}

			if (root.isMember("port"))
			{
				port = root.get("port", FLAG_port).asInt();
			}

			if (root.isMember("heartbeat"))
			{
				heartbeat = root.get("heartbeat", FLAG_heartbeat).asInt();
			}

			if (root.isMember("turn"))
			{
				auto turnNode = root.get("turn", NULL);

				if (turnNode.isMember("provider"))
				{
					turnCredentialUri = turnNode.get("provider", "").asString();
				}
			}

			if (root.isMember("authentication"))
			{
				auto authenticationNode = root.get("authentication", NULL);

				if (authenticationNode.isMember("authority"))
				{
					authInfo.authority = authenticationNode.get("authority", "").asString();
				}

				if (authenticationNode.isMember("resource"))
				{
					authInfo.resource = authenticationNode.get("resource", "").asString();
				}

				if (authenticationNode.isMember("clientId"))
				{
					authInfo.clientId = authenticationNode.get("clientId", "").asString();
				}

				if (authenticationNode.isMember("clientSecret"))
				{
					authInfo.clientSecret = authenticationNode.get("clientSecret", "").asString();
				}
			}
		}
	}

	return InitWebRTC(server, port, heartbeat, authInfo, turnCredentialUri);
#endif // TEST_RUNNER
}
