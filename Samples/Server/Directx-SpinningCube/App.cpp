#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "DirectXHelper.h"
#include "DeviceResources.h"

#ifdef STEREO_OUTPUT_MODE
#include <Microsoft.Perception.Simulation.h>

#include "HolographicAppMain.h"
#else // STEREO_OUTPUT_MODE
#include "CubeRenderer.h"
#endif //STEREO_OUTPUT_MODE

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
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace Toolkit3DLibrary;
using namespace Toolkit3DSample;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HWND									g_hWnd = nullptr;
DeviceResources*						g_deviceResources = nullptr;

#ifdef STEREO_OUTPUT_MODE
// Holographics remoting simulation
ComPtr<IPerceptionSimulationControl>	g_spPerceptionSimulationControl;
UINT64									g_lastFrameTimestamp = 0;
ComPtr<ISimulationStreamSink>			g_spStreamSink;
ComPtr<IPerceptionSimulationFrame>		g_spFrame;

// The holographic space the app will use for rendering.
HolographicSpace^						g_holographicSpace = nullptr;
std::unique_ptr<HolographicAppMain>		g_main;
#else // STEREO_OUTPUT_MODE
CubeRenderer*							g_cubeRenderer = nullptr;
DX::StepTimer                           g_timer;
#endif //STEREO_OUTPUT_MODE

#ifdef TEST_RUNNER
VideoTestRunner*						g_videoTestRunner = nullptr;
#else // TEST_RUNNER
VideoHelper*							g_videoHelper = nullptr;
#endif // TESTRUNNER

#ifdef STEREO_OUTPUT_MODE
class FrameGeneratedCallbackWrapper
	: public RuntimeClass<RuntimeClassFlags<ClassicCom>, IPerceptionSimulationFrameGeneratedCallback, FtmBase>
{
public:
	HRESULT RuntimeClassInitialize()
	{
		return S_OK;
	}

	STDMETHODIMP FrameGenerated(_In_ IPerceptionSimulationFrame* frame) override
	{
		ComPtr<ID3D11Texture2D> spTexture;
		INT64 timestamp = 0;
		ThrowIfFailed(frame->get_PredictionTargetTime(&timestamp));

		if (timestamp != g_lastFrameTimestamp)
		{
			g_lastFrameTimestamp = timestamp;
			ThrowIfFailed(frame->get_Frame(&spTexture));

			// Renders to swapchain's buffer
			ComPtr<ID3D11Device1> spDevice = g_deviceResources->GetD3DDevice();
			ComPtr<ID3D11DeviceContext> spContext = g_deviceResources->GetD3DDeviceContext();

			ComPtr<ID3D11Texture2D> spBackBuffer;
			ThrowIfFailed(g_deviceResources->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&spBackBuffer)));

			spContext->CopySubresourceRegion(
				spBackBuffer.Get(), // dest
				0,                  // dest subresource
				0, 0, 0,            // dest x, y, z
				spTexture.Get(),    // source
				0,                  // source subresource
				nullptr);           // source box, null means the entire resource

			spContext->CopySubresourceRegion(
				spBackBuffer.Get(),
				0,
				FRAME_BUFFER_WIDTH / 2, 0, 0,
				spTexture.Get(),
				1,
				nullptr);
		}

		g_spFrame = frame;
		return S_OK;
	}

private:
	Platform::WeakReference m_outerWeak;
};

#endif //STEREO_OUTPUT_MODE

//--------------------------------------------------------------------------------------
// Global Methods
//--------------------------------------------------------------------------------------
void InitResources(HWND handle)
{
	g_deviceResources = new DeviceResources();

#ifdef STEREO_OUTPUT_MODE
	// Checks for Perception Simulation Supported.
    if (!Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
		L"Windows.Perception.Automation.Core.PerceptionAutomationCoreContract", 1))
    {
        throw ref new Platform::NotImplementedException();
    }

    DX::ThrowIfFailed(InitializePerceptionSimulation(
		PerceptionSimulationControlFlags_None,
        IID_PPV_ARGS(&g_spPerceptionSimulationControl)));

	// Initializes the Holographic space and control stream.
    ComPtr<IUnknown> spUnkHolographicSpace;
    DX::ThrowIfFailed(g_spPerceptionSimulationControl->get_HolographicSpace(&spUnkHolographicSpace));
    g_holographicSpace = static_cast<Windows::Graphics::Holographic::HolographicSpace^>(
		reinterpret_cast<Platform::Object^>(spUnkHolographicSpace.Get()));

    DX::ThrowIfFailed(g_spPerceptionSimulationControl->get_ControlStream(
		&g_spStreamSink));

	// Sets frame generated callback.
	ComPtr<FrameGeneratedCallbackWrapper> spFrameGeneratedCallback;
	DX::ThrowIfFailed(MakeAndInitialize<FrameGeneratedCallbackWrapper>(&spFrameGeneratedCallback));
	DX::ThrowIfFailed(g_spPerceptionSimulationControl->SetFrameGeneratedCallback(spFrameGeneratedCallback.Get()));

	// Sets Holographic space.
	g_deviceResources->SetHolographicSpace(g_holographicSpace);
	g_main = std::make_unique<HolographicAppMain>(g_deviceResources);
	g_main->SetHolographicSpace(g_holographicSpace);

	// Sets window.
	g_deviceResources->SetWindow(handle);
#else // STEREO_OUTPUT_MODE
	// Sets window.
	g_deviceResources->SetWindow(handle);

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);
#endif // STEREO_OUTPUT_MODE

	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

	g_videoHelper->Initialize(g_deviceResources->GetSwapChain());
}

void CleanupResources()
{
	delete g_videoHelper;
	delete g_deviceResources;

#ifndef STEREO_OUTPUT_MODE
	delete g_cubeRenderer;
#endif // STEREO_OUTPUT_MODE
}

int count = 0;
void FrameUpdate()
{
#ifdef STEREO_OUTPUT_MODE
	byte buf[164] = { 0 };
	int dataLength = 164;
	int offset = 0;
	++count;

	// Header
	*((int*)(buf + 0))		= 1; // Stream type
	*((int*)(buf + 4))		= 2; // Version

	// Unknown
	*((int*)(buf + 8))		= 1;
	*((int*)(buf + 12))		= 12;

	// Unknown1 ???
	*((int*)(buf + 16))		= -185552 + count;

	// Unknown
	*((int*)(buf + 20))		= 33;
	*((int*)(buf + 24))		= 2;

	// Rotation matrix (Column major)
	*((float*)(buf + 28))	= 1.0f;
	*((float*)(buf + 32))	= 0.0f;
	*((float*)(buf + 36))	= 0.0f;
	*((float*)(buf + 40))	= 0.0f;
	*((float*)(buf + 44))	= 1.0f;
	*((float*)(buf + 48))	= 0.0f;
	*((float*)(buf + 52))	= 0.0f;
	*((float*)(buf + 56))	= 0.0f;
	*((float*)(buf + 60))	= 1.0f;

	// Translation vector
	*((float*)(buf + 64))	= 0.0f;
	*((float*)(buf + 68))	= 0.0f;
	*((float*)(buf + 72))	= 0.0f;

	// 76 - 80: ignored

	// Unknown2 ???
	*((int*)(buf + 80))		= -79896 + count;

	// Unknown
	*((int*)(buf + 84))		= 33;
	*((int*)(buf + 88))		= 2;

	// Rotation matrix (Column major)
	*((float*)(buf + 92))	= 1.0f;
	*((float*)(buf + 96))	= 0.0f;
	*((float*)(buf + 100))	= 0.0f;
	*((float*)(buf + 104))	= 0.0f;
	*((float*)(buf + 108))	= 1.0f;
	*((float*)(buf + 112))	= 0.0f;
	*((float*)(buf + 116))	= 0.0f;
	*((float*)(buf + 120))	= 0.0f;
	*((float*)(buf + 124))	= 1.0f;

	// Translation vector
	*((float*)(buf + 128))	= 0.0f;
	*((float*)(buf + 132))	= 0.0f;
	*((float*)(buf + 136))	= 0.0f;

	// 140 - 144: ignored

	// Unknown 3
	*((int*)(buf + 144))	= 118363 + count;
	*((int*)(buf + 148))	= 104577;

	// Unknown
	*((int*)(buf + 152))	= 0;
	*((int*)(buf + 158))	= 64;
	*((int*)(buf + 160))	= 36;

	g_spStreamSink->OnPacketReceived(dataLength, buf);

	if (g_main)
	{
		// When running on Windows Holographic, we can use the holographic rendering system.
		HolographicFrame^ holographicFrame = g_main->Update();

		if (holographicFrame && g_main->Render(holographicFrame))
		{
			// The holographic frame has an API that presents the swap chain for each
			// holographic camera.
			g_deviceResources->Present(holographicFrame);
		}
	}
#else // STEREO_OUTPUT_MODE
	g_timer.Tick([&]()
	{
		//
		// Update scene objects.
		//
		// Put time-based updates here. By default this code will run once per frame,
		// but if you change the StepTimer to use a fixed time step this code will
		// run as many times as needed to get to the current step.
		//
		g_cubeRenderer->Update(g_timer);
	});

	g_cubeRenderer->Render();
#endif // STEREO_OUTPUT_MODE
}

#ifdef REMOTE_RENDERING

// Handles input from client.
void InputUpdate(const std::string& message)
{
	char data[1024];
	Json::Reader reader;
	Json::Value msg = NULL;
	reader.parse(message, msg, false);

	if (msg.isMember("type"))
	{
		strcpy(data, msg.get("type", "").asCString());

#ifdef STEREO_OUTPUT_MODE
		if (strcmp(data, "camera-transform-stereo") == 0)
		{
			if (msg.isMember("body"))
			{
				strcpy(data, msg.get("body", "").asCString());

				// Parses the camera transformation data.
				std::istringstream datastream(data);
				std::string token;

				// Parses the left view matrix.
				DirectX::XMFLOAT4X4 viewLeft;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewLeft.m[i][j] = stof(token);
					}
				}

				// Parses the right view matrix.
				DirectX::XMFLOAT4X4 viewRight;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						getline(datastream, token, ',');
						viewRight.m[i][j] = stof(token);
					}
				}
			}
		}
#endif // STEREO_OUTPUT_MODE
	}
}

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------
int InitWebRTC(char* server, int port)
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

	// Initializes resources.
	InitResources(wnd.handle());

	rtc::InitializeSSL();
	PeerConnectionClient client;

	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(
			&client, &wnd, &FrameUpdate, &InputUpdate, g_videoHelper));

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
	CleanupResources();

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
HRESULT InitWindow()
{
	// Registers class.
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = 0;
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
		0,
		nullptr);

	if (!g_hWnd)
	{
		return E_FAIL;
	}

	ShowWindow(g_hWnd, SW_SHOWNORMAL);

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
int main(Platform::Array<Platform::String^>^ args)
{
	RoInitializeWrapper roinit(RO_INIT_MULTITHREADED);
	DX::ThrowIfFailed(HRESULT(roinit));

#ifndef REMOTE_RENDERING
	if (FAILED(InitWindow()))
	{
		return 0;
	}

	// Initializes the device resources.
	InitResources(g_hWnd);

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

	// Cleanup.
	CleanupResources();

	return (int)msg.wParam;
#else // REMOTE_RENDERING
	char server[1024];
	strcpy(server, FLAG_server);
	int port = FLAG_port;

	// Try parsing command line arguments.
	if (args->Length == 3)
	{
		wcstombs(server, args[1]->Data(), sizeof(server));
		port = _wtoi(args[2]->Data());
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
		}
	}

	return InitWebRTC(server, port);
#endif // REMOTE_RENDERING
}
