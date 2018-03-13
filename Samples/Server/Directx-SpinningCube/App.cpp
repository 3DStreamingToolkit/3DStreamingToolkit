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
#include "config_parser.h"
#include "directx_multi_peer_conductor.h"
#include "server_main_window.h"
#include "server_renderer.h"
#include "service/render_service.h"
#include "webrtc.h"
#endif // TEST_RUNNER

// Position the cube two meters in front of user for image stabilization.
#define FOCUS_POINT					-2.0f

// If clients don't send "stereo-rendering" message after this time,
// the video stream will start in non-stereo mode.
#define STEREO_FLAG_WAIT_TIME		3000

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
DeviceResources*					g_deviceResources = nullptr;
CubeRenderer*						g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
HWND								g_hWnd = nullptr;
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

	// The projection matrix for left eye used in camera transform
	DirectX::XMFLOAT4X4				projectionMatrixLeft;

	// The view matrix for left eye used in camera transform
	DirectX::XMFLOAT4X4				viewMatrixLeft;

	// The projection matrix for right eye used in camera transform
	DirectX::XMFLOAT4X4				projectionMatrixRight;

	// The view matrix for right eye used in camera transform
	DirectX::XMFLOAT4X4				viewMatrixRight;

	// The timestamp used for frame synchronization in stereo mode
	int64_t							lastTimestamp;

	// The render texture which we use to render
	ComPtr<ID3D11Texture2D>			renderTexture;

	// The render target view of the render texture
	ComPtr<ID3D11RenderTargetView>	renderTargetView;

	// The depth stencil texture which we use to render
	ComPtr<ID3D11Texture2D>			depthStencilTexture;

	// The depth stencil view of the depth stencil texture
	ComPtr<ID3D11DepthStencilView>	depthStencilView;

	// Used for FPS limiter.
	ULONGLONG						tick;

	// The starting time.
	ULONGLONG						startTick;
};

std::map<int, std::shared_ptr<RemotePeerData>> g_remotePeersData;
#endif // TESTRUNNER

#ifndef TEST_RUNNER

void InitializeRenderTexture(RemotePeerData* peerData, int width, int height, bool isStereo)
{
	int texWidth = isStereo ? width << 1 : width;
	int texHeight = height;

	// Creates the render texture.
	D3D11_TEXTURE2D_DESC texDesc = { 0 };
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	g_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &peerData->renderTexture);

	// Creates the render target view.
	g_deviceResources->GetD3DDevice()->CreateRenderTargetView(peerData->renderTexture.Get(), nullptr, &peerData->renderTargetView);
}

void InitializeDepthStencilTexture(RemotePeerData* peerData, int width, int height, bool isStereo)
{
	int texWidth = isStereo ? width << 1 : width;
	int texHeight = height;

	// Creates the depth stencil texture.
	D3D11_TEXTURE2D_DESC texDesc = { 0 };
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.Width = texWidth;
	texDesc.Height = texHeight;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	g_deviceResources->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &peerData->depthStencilTexture);

	// Creates the depth stencil view.
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = texDesc.Format;
	descDSV.Flags = 0;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	g_deviceResources->GetD3DDevice()->CreateDepthStencilView(peerData->depthStencilTexture.Get(), &descDSV, &peerData->depthStencilView);
}

bool AppMain(BOOL stopping)
{
	auto fullServerConfig = GlobalObject<FullServerConfig>::Get();
	auto nvEncConfig = GlobalObject<NvEncConfig>::Get();

	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

	ServerMainWindow wnd(
		fullServerConfig->webrtc_config->server.c_str(),
		fullServerConfig->webrtc_config->port,
		fullServerConfig->server_config->server_config.auto_connect,
		fullServerConfig->server_config->server_config.auto_call,
		false,
		fullServerConfig->server_config->server_config.width,
		fullServerConfig->server_config->server_config.height);

	if (!fullServerConfig->server_config->server_config.system_service && !wnd.Create())
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
	DirectXMultiPeerConductor cond(fullServerConfig, g_deviceResources->GetD3DDevice());

	// Sets main window to update UI.
	cond.SetMainWindow(&wnd);

	// Registers the handler.
	wnd.RegisterObserver(&cond);

	// Handles data channel messages.
	std::function<void(int, const string&)> dataChannelMessageHandler([&](
		int peerId,
		const std::string& message)
	{
		// Returns if the remote peer data hasn't been initialized.
		if (g_remotePeersData.find(peerId) == g_remotePeersData.end())
		{
			return;
		}

		char type[256];
		char body[1024];
		Json::Reader reader;
		Json::Value msg = NULL;
		reader.parse(message, msg, false);
		std::shared_ptr<RemotePeerData> peerData = g_remotePeersData[peerId];
		if (msg.isMember("type") && msg.isMember("body"))
		{
			strcpy(type, msg.get("type", "").asCString());
			strcpy(body, msg.get("body", "").asCString());
			std::istringstream datastream(body);
			std::string token;
			if (strcmp(type, "stereo-rendering") == 0 && !peerData->renderTexture)
			{
				getline(datastream, token, ',');
				peerData->isStereo = stoi(token) == 1;
				InitializeRenderTexture(
					peerData.get(),
					fullServerConfig->server_config->server_config.width,
					fullServerConfig->server_config->server_config.height,
					peerData->isStereo);

				InitializeDepthStencilTexture(
					peerData.get(),
					fullServerConfig->server_config->server_config.width,
					fullServerConfig->server_config->server_config.height,
					peerData->isStereo);

				if (!peerData->isStereo)
				{
					peerData->eyeVector = g_cubeRenderer->GetDefaultEyeVector();
					peerData->lookAtVector = g_cubeRenderer->GetDefaultLookAtVector();
					peerData->upVector = g_cubeRenderer->GetDefaultUpVector();
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

				peerData->projectionMatrixLeft = projectionMatrixLeft;
				peerData->viewMatrixLeft = viewMatrixLeft;
				peerData->projectionMatrixRight = projectionMatrixRight;
				peerData->viewMatrixRight= viewMatrixRight;
				peerData->isNew = true;
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
				if (timestamp != peerData->lastTimestamp)
				{
					peerData->lastTimestamp = timestamp;
					peerData->projectionMatrixLeft = projectionMatrixLeft;
					peerData->viewMatrixLeft = viewMatrixLeft;
					peerData->projectionMatrixRight = projectionMatrixRight;
					peerData->viewMatrixRight = viewMatrixRight;
					peerData->isNew = true;
				}
			}
		}
	});

	// Sets data channel message handler.
	cond.SetDataChannelMessageHandler(dataChannelMessageHandler);

	// Main loop.
	MSG msg = { 0 };
	while (!stopping && WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (fullServerConfig->server_config->server_config.system_service ||
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

				// Retrieves remote peer data from map, create new if needed.
				std::shared_ptr<RemotePeerData> peerData;
				auto it = g_remotePeersData.find(peer->Id());
				if (it == g_remotePeersData.end())
				{
					peerData.reset(new RemotePeerData());
					peerData->startTick = GetTickCount64();
					g_remotePeersData[peer->Id()] = peerData;
				}
				else
				{
					peerData = it->second;
				}

				if (!peerData->renderTexture)
				{
					// Forces non-stereo mode initialization.
					if (GetTickCount64() - peerData->startTick >= STEREO_FLAG_WAIT_TIME)
					{
						InitializeRenderTexture(
							peerData.get(),
							fullServerConfig->server_config->server_config.width,
							fullServerConfig->server_config->server_config.height,
							false);

						InitializeDepthStencilTexture(
							peerData.get(),
							fullServerConfig->server_config->server_config.width,
							fullServerConfig->server_config->server_config.height,
							false);

						peerData->isStereo = false;
						peerData->eyeVector = g_cubeRenderer->GetDefaultEyeVector();
						peerData->lookAtVector = g_cubeRenderer->GetDefaultLookAtVector();
						peerData->upVector = g_cubeRenderer->GetDefaultUpVector();
						peerData->tick = GetTickCount64();
					}
				}
				else
				{
					g_deviceResources->SetStereo(peerData->isStereo);
					if (!peerData->isStereo)
					{
						// FPS limiter.
						const int interval = 1000 / nvEncConfig->capture_fps;
						ULONGLONG timeElapsed = GetTickCount64() - peerData->tick;
						if (timeElapsed >= interval)
						{
							peerData->tick = GetTickCount64() - timeElapsed + interval;
							g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, 0.f }));
							g_cubeRenderer->UpdateView(
								peerData->eyeVector,
								peerData->lookAtVector,
								peerData->upVector);

							g_cubeRenderer->Render(peerData->renderTargetView.Get());
							peer->SendFrame(peerData->renderTexture.Get());
						}
					}
					// In stereo rendering mode, we only update frame whenever
					// receiving any input data.
					else if (peerData->isNew)
					{
						g_cubeRenderer->SetPosition(float3({ 0.f, 0.f, FOCUS_POINT }));

						DirectX::XMFLOAT4X4 leftMatrix;
						XMStoreFloat4x4(
							&leftMatrix,
							XMLoadFloat4x4(&peerData->projectionMatrixLeft) * XMLoadFloat4x4(&peerData->viewMatrixLeft));

						DirectX::XMFLOAT4X4 rightMatrix;
						XMStoreFloat4x4(
							&rightMatrix,
							XMLoadFloat4x4(&peerData->projectionMatrixRight) * XMLoadFloat4x4(&peerData->viewMatrixRight));

						g_cubeRenderer->UpdateView(leftMatrix, rightMatrix);
						g_cubeRenderer->Render(peerData->renderTargetView.Get());
						peer->SendFrame(peerData->renderTexture.Get(), peerData->lastTimestamp);
						peerData->isNew = false;
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
	g_cubeRenderer->UpdateView(
		g_cubeRenderer->GetDefaultEyeVector(),
		g_cubeRenderer->GetDefaultLookAtVector(),
		g_cubeRenderer->GetDefaultUpVector());

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