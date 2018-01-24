#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "macros.h"
#include "CubeRenderer.h"
#include "DeviceResources.h"

#ifdef TEST_RUNNER
#include "test_runner.h"
#else // TEST_RUNNER
#include "server_main_window.h"
#include "server_authentication_provider.h"
#include "turn_credential_provider.h"
#include "server_renderer.h"
#include "webrtc.h"
#include "config_parser.h"
#include "service/render_service.h"
#include "multi_peer_conductor.h"
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
HWND								g_hWnd = nullptr;
DeviceResources*					g_deviceResources = nullptr;
CubeRenderer*						g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
VideoTestRunner*					g_videoTestRunner = nullptr;
#else // TEST_RUNNER
// Remote peer data
struct RemotePeerData
{
	// True if this data hasn't been processed
	bool							isNew;

	// True for stereo output, false otherwise
	bool							isStereo;

	// The look at vector used in camera transform
	DirectX::XMVECTORF32			lookAtVector;

	// The up vector used in camera transform
	DirectX::XMVECTORF32			upVector;

	// The eye vector used in camera transform
	DirectX::XMVECTORF32			eyeVector;

	// The view-projection matrix for left eye used in camera transform
	DirectX::XMFLOAT4X4				viewProjectionMatrixLeft;

	// The view-projection matrix for right eye used in camera transform
	DirectX::XMFLOAT4X4				viewProjectionMatrixRight;

	// The timestamp used for frame synchronization in stereo mode
	int64_t							lastTimestamp;

	// The render texture which we use to render
	ComPtr<ID3D11Texture2D>			renderTexture;

	// The render target view of the render texture
	ComPtr<ID3D11RenderTargetView>	renderTextureRtv;

	// Used for FPS limiter.
	ULONGLONG						tick;
};

std::map<int, std::shared_ptr<RemotePeerData>> g_remotePeersData;
#endif // TESTRUNNER

#ifndef TEST_RUNNER

void InitializeRenderTextures(RemotePeerData* peerData, int width, int height, bool isStereo)
{
	int texWidth = isStereo ? width << 1 : width;
	int texHeight = height;
	D3D11_TEXTURE2D_DESC texDesc = { 0 };
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	// Creates texture and render target view
	g_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &peerData->renderTexture);
	g_deviceResources->GetD3DDevice()->CreateRenderTargetView(peerData->renderTexture.Get(), nullptr, &peerData->renderTextureRtv);
}

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

	// Initializes SSL.
	rtc::InitializeSSL();

	// Initializes the conductor.
	MultiPeerConductor cond(webrtcConfig,
		g_deviceResources->GetD3DDevice(),
		nvEncConfig->use_software_encoding);

	// Handles data channel messages.
	std::function<void(int, const string&)> dataChannelMessageHandler([&](
		int peerId,
		const std::string& message)
	{
		char type[256];
		char body[1024];
		Json::Reader reader;
		Json::Value msg = NULL;
		reader.parse(message, msg, false);

		// Retrieves remote peer data from map, create new if needed.
		std::shared_ptr<RemotePeerData> peerData;
		auto it = g_remotePeersData.find(peerId);
		if (it != g_remotePeersData.end())
		{
			peerData = it->second;
		}
		else
		{
			peerData.reset(new RemotePeerData());
			g_remotePeersData[peerId] = peerData;
		}

		if (msg.isMember("type") && msg.isMember("body"))
		{
			strcpy(type, msg.get("type", "").asCString());
			strcpy(body, msg.get("body", "").asCString());
			std::istringstream datastream(body);
			std::string token;
			if (strcmp(type, "stereo-rendering") == 0)
			{
				getline(datastream, token, ',');
				peerData->isStereo = stoi(token) == 1;
				InitializeRenderTextures(
					peerData.get(),
					serverConfig->server_config.width,
					serverConfig->server_config.height,
					peerData->isStereo);

				if (peerData->isStereo)
				{
					// In stereo rendering mode, we need to position the cube
					// in front of user.
					g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, FOCUS_POINT }));
				}
				else
				{
					g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, 0.f }));
					peerData->tick = GetTickCount64();
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

				peerData->lookAtVector = { focusX, focusY, focusZ, 0.f };
				peerData->upVector = { upX, upY, upZ, 0.f };
				peerData->eyeVector = { eyeX, eyeY, eyeZ, 0.f };
				peerData->isNew = true;
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

				peerData->viewProjectionMatrixLeft = viewProjectionLeft;
				peerData->viewProjectionMatrixRight = viewProjectionRight;
				peerData->isNew = true;
			}
			else if (strcmp(type, "camera-transform-stereo-prediction") == 0)
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

				// Parses the prediction timestamp.
				getline(datastream, token, ',');
				int64_t timestamp = stoll(token);
				if (timestamp != peerData->lastTimestamp)
				{
					peerData->lastTimestamp = timestamp;
					peerData->viewProjectionMatrixLeft = viewProjectionLeft;
					peerData->viewProjectionMatrixRight = viewProjectionRight;
					peerData->isNew = true;
				}
			}
		}
	});

	// Phong: TODO
	// if (serverConfig->server_config.system_service)
	{
		cond.ConnectSignallingAsync("renderingserver_test");
	}

	cond.SetDataChannelMessageHandler(dataChannelMessageHandler);

	// Main loop.
	while (!stopping)
	{
		MSG msg = { 0 };

		// if we're quitting, do so
		if (wndHandler.isClosing)
		{
			break;
		}

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
			for each (auto pair in cond.Peers())
			{
				auto peer = pair.second;
				auto it = g_remotePeersData.find(peer->Id());
				if (it != g_remotePeersData.end())
				{
					RemotePeerData* peerData = it->second.get();
					if (peerData->renderTexture)
					{
						if (!peerData->isStereo)
						{
							// FPS limiter.
							const int interval = 1000 / nvEncConfig->capture_fps;
							ULONGLONG timeElapsed = GetTickCount64() - peerData->tick;
							if (timeElapsed >= interval)
							{
								peerData->tick = GetTickCount64() - timeElapsed + interval;
								if (peerData->isNew)
								{
									g_cubeRenderer->Update(
										peerData->eyeVector,
										peerData->lookAtVector,
										peerData->upVector);
								}
								else
								{
									g_cubeRenderer->Update();
								}

								g_cubeRenderer->Render(peerData->renderTextureRtv.Get());
								peer->SendFrame(peerData->renderTexture.Get());
							}
						}
						// In stereo rendering mode, we only update frame whenever
						// receiving any input data.
						else if (peerData->isNew)
						{
							g_cubeRenderer->Update(
								peerData->viewProjectionMatrixLeft,
								peerData->viewProjectionMatrixRight);

							g_cubeRenderer->Render(peerData->renderTextureRtv.Get());
							peer->SendFrame(peerData->renderTexture.Get(), peerData->lastTimestamp);
							peerData->isNew = false;
						}
					}
				}
			}
		}
	}

	// Cleanup.
	rtc::CleanupSSL();
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