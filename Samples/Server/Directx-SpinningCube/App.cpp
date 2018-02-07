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
DirectX::XMFLOAT4X4		g_projectionMatrixLeft;
DirectX::XMFLOAT4X4		g_viewMatrixLeft;
DirectX::XMFLOAT4X4		g_projectionMatrixRight;
DirectX::XMFLOAT4X4		g_viewMatrixRight;
#endif // TESTRUNNER

#ifndef TEST_RUNNER

bool AppMain(BOOL stopping)
{
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();
	auto serverConfig = GlobalObject<ServerConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	ServerAuthenticationProvider::ServerAuthInfo authInfo;
	authInfo.authority = webrtcConfig->authentication.authority;
	authInfo.resource = webrtcConfig->authentication.resource;
	authInfo.clientId = webrtcConfig->authentication.client_id;
	authInfo.clientSecret = webrtcConfig->authentication.client_secret;

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

		if (FAILED(hr))
		{
			return hr;
		}
	}

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
				bool isStereo = stoi(token) == 1;
				if (isStereo != g_deviceResources->IsStereo())
				{
					// Resizes the swap chain.
					frameBuffer.Reset();
					g_deviceResources->SetStereo(isStereo);
					if (!serverConfig->server_config.system_service)
					{
						HRESULT hr = g_deviceResources->GetSwapChain()->GetBuffer(
							0,
							__uuidof(ID3D11Texture2D),
							reinterpret_cast<void**>(frameBuffer.GetAddressOf()));

						if (FAILED(hr))
						{
							return hr;
						}
					}
					else
					{
						SIZE size = g_deviceResources->GetOutputSize();
						bufferCapturer->ResizeRenderTexture(size.cx, size.cy);
					}

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

				g_lookAtVector = { focusX, focusY, focusZ, 0.f };
				g_upVector = { upX, upY, upZ, 0.f };
				g_eyeVector = { eyeX, eyeY, eyeZ, 0.f };
				g_hasNewInputData = true;
			}
			else if (strcmp(type, "camera-transform-stereo") == 0)
			{
				// Parses the left projection matrix.
				DirectX::XMFLOAT4X4 projectionMatrixLeft;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						projectionMatrixLeft.m[i][j] = stof(token);
					}
				}

				// Parses the left view matrix.
				DirectX::XMFLOAT4X4 viewMatrixLeft;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewMatrixLeft.m[i][j] = stof(token);
					}
				}

				// Parses the right projection matrix.
				DirectX::XMFLOAT4X4 projectionMatrixRight;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						projectionMatrixRight.m[i][j] = stof(token);
					}
				}

				// Parses the right view matrix.
				DirectX::XMFLOAT4X4 viewMatrixRight;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewMatrixRight.m[i][j] = stof(token);
					}
				}

				g_projectionMatrixLeft = projectionMatrixLeft;
				g_viewMatrixLeft = viewMatrixLeft;
				g_projectionMatrixRight = projectionMatrixRight;
				g_viewMatrixRight = viewMatrixRight;
				g_hasNewInputData = true;
			}
			else if (strcmp(type, "camera-transform-stereo-prediction") == 0)
			{
				// Parses the left projection matrix.
				DirectX::XMFLOAT4X4 projectionMatrixLeft;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						projectionMatrixLeft.m[i][j] = stof(token);
					}
				}

				// Parses the left view matrix.
				DirectX::XMFLOAT4X4 viewMatrixLeft;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewMatrixLeft.m[i][j] = stof(token);
					}
				}

				// Parses the right projection matrix.
				DirectX::XMFLOAT4X4 projectionMatrixRight;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						projectionMatrixRight.m[i][j] = stof(token);
					}
				}

				// Parses the right view matrix.
				DirectX::XMFLOAT4X4 viewMatrixRight;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewMatrixRight.m[i][j] = stof(token);
					}
				}

				// Parses the prediction timestamp.
				getline(datastream, token, ',');
				int64_t timestamp = stoll(token);
				if (timestamp != g_lastTimestamp)
				{
					g_lastTimestamp = timestamp;
					g_projectionMatrixLeft = projectionMatrixLeft;
					g_viewMatrixLeft = viewMatrixLeft;
					g_projectionMatrixRight = projectionMatrixRight;
					g_viewMatrixRight = viewMatrixRight;
					g_hasNewInputData = true;
				}
			}
		}
	});

	conductor->SetInputDataHandler(&inputHandler);
	client.SetHeartbeatMs(webrtcConfig->heartbeat);

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

			// For system service, automatically connect to the signaling server
			// after successful authentication.
			if (serverConfig->server_config.system_service)
			{
				conductor->StartLogin(webrtcConfig->server, webrtcConfig->port);
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
	else if (serverConfig->server_config.system_service)
	{
		// For system service, automatically connect to the signaling server.
		conductor->StartLogin(webrtcConfig->server, webrtcConfig->port);
	}

	// configure turn, if needed
	if (!webrtcConfig->turn_server.provider.empty())
	{
		turnProvider.reset(new TurnCredentialProvider(webrtcConfig->turn_server.provider));
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
	if (serverConfig->server_config.system_service)
	{
		conductor->StartLogin(webrtcConfig->server, webrtcConfig->port);
	}

	// Main loop.
	while (!stopping)
	{
		MSG msg = { 0 };

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (serverConfig->server_config.system_service ||
				!wnd.PreTranslateMessage(&msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (conductor->is_closing())
			{
				break;
			}

			if (conductor->connection_active() || client.is_connected())
			{
				ULONGLONG tick = GetTickCount64();
				if (!g_deviceResources->IsStereo())
				{
					if (g_hasNewInputData)
					{
						g_cubeRenderer->Update(g_eyeVector, g_lookAtVector, g_upVector);
						g_hasNewInputData = false;
					}
					else
					{
						g_cubeRenderer->Update();
					}

					// For system service, we render to buffer instead of swap chain.
					if (serverConfig->server_config.system_service)
					{
						g_cubeRenderer->Render(bufferCapturer->GetRenderTargetView());
						bufferCapturer->SendFrame();
					}
					else
					{
						g_cubeRenderer->Render();
						bufferCapturer->SendFrame(frameBuffer.Get());
						g_deviceResources->Present();
					}

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
				// In stereo rendering mode, we only update frame whenever
				// receiving any input data.
				else if (g_hasNewInputData)
				{
					DirectX::XMFLOAT4X4 leftMatrix;
					DirectX::XMFLOAT4X4 rightMatrix;

					XMStoreFloat4x4(
						&leftMatrix,
						XMLoadFloat4x4(&g_projectionMatrixLeft) * XMLoadFloat4x4(&g_viewMatrixLeft));

					XMStoreFloat4x4(
						&rightMatrix,
						XMLoadFloat4x4(&g_projectionMatrixRight) * XMLoadFloat4x4(&g_viewMatrixRight));

					g_cubeRenderer->Update(leftMatrix, rightMatrix);

					// For system service, we render to buffer instead of swap chain.
					if (serverConfig->server_config.system_service)
					{
						g_cubeRenderer->Render(bufferCapturer->GetRenderTargetView());
						bufferCapturer->SendFrame(g_lastTimestamp);
					}
					else
					{
						g_cubeRenderer->Render();
						bufferCapturer->SendFrame(frameBuffer.Get(), g_lastTimestamp);
						//g_deviceResources->Present();
					}

					g_hasNewInputData = false;
				}
			}
		}
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
