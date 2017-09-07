#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "DeviceResources.h"
#include "CubeRenderer.h"

#ifdef TEST_RUNNER
#include "test_runner.h"
#else // TEST_RUNNER
#include "server_main_window.h"
#include "server_authentication_provider.h"
#include "turn_credential_provider.h"
#include "server_renderer.h"
#include "webrtc.h"
#include "structs.h"
#include "config_parser.h"
#include "service/render_service.h"
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
using namespace Microsoft::WRL;
using namespace StreamingToolkit;
using namespace StreamingToolkitSample;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
void RegisterService();
void RemoveService();

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HWND				g_hWnd = nullptr;
DeviceResources*	g_deviceResources = nullptr;
CubeRenderer*		g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
VideoTestRunner*	g_videoTestRunner = nullptr;
#else // TEST_RUNNER
BufferRenderer*		g_bufferRenderer = nullptr;
ServerConfig		g_serverConfig;
ServiceConfig		g_serviceConfig;
WebRTCConfig		g_webrtcConfig;
#endif // TESTRUNNER

#ifndef TEST_RUNNER

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------

void LoadConfigs()
{
	// Loads server config file.
	ConfigParser::Parse("serverConfig.json", &g_serverConfig, &g_serviceConfig);

	// Loads webrtc config file.
	ConfigParser::Parse("webrtcConfig.json", &g_webrtcConfig);
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
bool AppMain(BOOL stopping)
{
	ServerAuthenticationProvider::ServerAuthInfo authInfo;
	authInfo.authority = g_webrtcConfig.authentication.authority;
	authInfo.resource = g_webrtcConfig.authentication.resource;
	authInfo.clientId = g_webrtcConfig.authentication.client_id;
	authInfo.clientSecret = g_webrtcConfig.authentication.client_secret;

	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

#ifdef NO_UI
	ServerMainWindow wnd(server, port, true, true, true);
#else // NO_UI
	ServerMainWindow wnd(
		g_webrtcConfig.server.c_str(),
		g_webrtcConfig.port,
		FLAG_autoconnect,
		FLAG_autocall,
		false,
		g_serverConfig.width,
		g_serverConfig.height);
#endif // NO_UI

	if (!g_serverConfig.system_service && !wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(wnd.handle());

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	// Render loop.
	std::function<void()> frameRenderFunc = ([&]
	{
		g_cubeRenderer->Update();

		// For system service, we render to buffer instead of swap chain.
		if (g_serverConfig.system_service)
		{
			g_cubeRenderer->Render(g_bufferRenderer->GetRenderTargetView());
		}
		else
		{
			g_cubeRenderer->Render();
		}
	});

	ComPtr<ID3D11Texture2D> frameBuffer;
	if (!g_serverConfig.system_service)
	{
		// Gets the frame buffer from the swap chain.
		HRESULT hr = g_deviceResources->GetSwapChain()->GetBuffer(0,
			__uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(frameBuffer.GetAddressOf()));
	}

	// Initializes the buffer renderer.
	g_bufferRenderer = new BufferRenderer(
		g_serverConfig.width,
		g_serverConfig.height,
		g_deviceResources->GetD3DDevice(),
		frameRenderFunc,
		frameBuffer.Get());

	rtc::InitializeSSL();

	std::shared_ptr<ServerAuthenticationProvider> authProvider;
	std::shared_ptr<TurnCredentialProvider> turnProvider;
	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(
		&client, &wnd, g_bufferRenderer));

	// Handles input from client.
	InputDataHandler inputHandler([&](const std::string& message)
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
	});

	conductor->SetInputDataHandler(&inputHandler);
	client.SetHeartbeatMs(g_webrtcConfig.heartbeat);

	// configure callbacks (which may or may not be used)
	AuthenticationProvider::AuthenticationCompleteCallback authComplete([&](const AuthenticationProviderResult& data)
	{
		if (data.successFlag)
		{
			client.SetAuthorizationHeader("Bearer " + data.accessToken);

			// indicate to the user auth is complete (only if turn isn't in play)
			if (turnProvider.get() == nullptr)
			{
				wnd.SetAuthCode(L"OK");
			}
		}
	});

	TurnCredentialProvider::CredentialsRetrievedCallback credentialsRetrieved([&](const TurnCredentials& creds)
	{
		if (creds.successFlag)
		{
			conductor->SetTurnCredentials(creds.username, creds.password);

			// indicate to the user turn is done
			wnd.SetAuthCode(L"OK");
		}
	});

	// configure auth, if needed
	if (!authInfo.authority.empty())
	{
		authProvider.reset(new ServerAuthenticationProvider(authInfo));

		authProvider->SignalAuthenticationComplete.connect(&authComplete, &AuthenticationProvider::AuthenticationCompleteCallback::Handle);
	}

	// configure turn, if needed
	if (!g_webrtcConfig.turn_server.provider.empty())
	{
		turnProvider.reset(new TurnCredentialProvider(g_webrtcConfig.turn_server.provider));
		turnProvider->SignalCredentialsRetrieved.connect(
			&credentialsRetrieved,
			&TurnCredentialProvider::CredentialsRetrievedCallback::Handle);
	}

	// start auth or turn if needed
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

	// let the user know what we're doing
	if (turnProvider.get() != nullptr || authProvider.get() != nullptr)
	{
		if (authProvider.get() != nullptr)
		{
			wnd.SetAuthUri(std::wstring(authInfo.authority.begin(), authInfo.authority.end()));
		}

		wnd.SetAuthCode(L"Loading");
	}
	else
	{
		wnd.SetAuthCode(L"Not configured");
		wnd.SetAuthUri(L"Not configured");
	}

	// For system service, automatically connect to the signaling server.
	if (g_serverConfig.system_service)
	{
		conductor->StartLogin(g_webrtcConfig.server, g_webrtcConfig.port);
	}

	// Main loop.
	MSG msg;
	BOOL gm;
	while (!stopping && (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
	{
		// For system service, ignore window and swap chain.
		if (g_serverConfig.system_service)
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		else
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
	}

	rtc::CleanupSSL();

	// Cleanup.
	delete g_bufferRenderer;
	delete g_cubeRenderer;
	delete g_deviceResources;

	return 0;
}

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
	// Loads all config files.
	LoadConfigs();

	if (!g_serverConfig.system_service)
	{
		// If the app isn't defined as a system service, remove it if present.
		RemoveService();

		// Starts the app main.
		return AppMain(FALSE);
	}
	else
	{
		RegisterService();
		return 0;
	}
#endif // TEST_RUNNER
}

//--------------------------------------------------------------------------------------
// System service
//--------------------------------------------------------------------------------------
void RegisterService()
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager)
	{
		SC_HANDLE schService = OpenService(
			schSCManager,
			g_serviceConfig.name.c_str(),
			SERVICE_QUERY_STATUS);

		if (schService == NULL)
		{
			// If the service isn't already present, install it.
			RenderService::InstallService(
				g_serviceConfig.name,
				g_serviceConfig.display_name,
				g_serviceConfig.service_account,
				g_serviceConfig.service_password);
		}

		CloseServiceHandle(schService);
		schService = NULL;

		// Init service.
		const std::function<void(BOOL*)> serviceMainFunc = [&](BOOL* stopping)
		{
			AppMain(*stopping);
		};

		RenderService service((PWSTR)g_serviceConfig.name.c_str(), serviceMainFunc);

		// Starts the service to run the app persistently.
		if (!CServiceBase::Run(service))
		{
			wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
		}

		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
}

void RemoveService()
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager)
	{
		SC_HANDLE schService = OpenService(
			schSCManager,
			g_serviceConfig.name.c_str(),
			SERVICE_QUERY_STATUS);

		if (schService)
		{
			RenderService::RemoveService(g_serviceConfig.name.c_str());
		}

		CloseServiceHandle(schService);
		schService = NULL;

		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
}
