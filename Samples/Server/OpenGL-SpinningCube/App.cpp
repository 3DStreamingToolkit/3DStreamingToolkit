#include "pch.h"

#include <fstream>
#include <stdlib.h>
#include <shellapi.h>

#include "config_parser.h"
#include "CubeRenderer.h"
#include "macros.h"
#include "opengl_multi_peer_conductor.h"
#include "server_main_window.h"
#include "service/render_service.h"
#include "webrtc.h"

// If clients don't send "stereo-rendering" message after this time,
// the video stream will start in non-stereo mode.
#define STEREO_FLAG_WAIT_TIME		3000

// Required app libs
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "winmm.lib")

using namespace Microsoft::WRL;
using namespace StreamingToolkit;
using namespace StreamingToolkitSample;

//--------------------------------------------------------------------------------------
// OpenGL API defines
//--------------------------------------------------------------------------------------

// Function pointers for WGL_EXT_swap_control
#ifdef _WIN32
typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
PFNWGLSWAPINTERVALEXTPROC			pwglSwapIntervalEXT = 0;
PFNWGLGETSWAPINTERVALEXTPROC		pwglGetSwapIntervalEXT = 0;
#define wglSwapIntervalEXT			pwglSwapIntervalEXT
#define wglGetSwapIntervalEXT		pwglGetSwapIntervalEXT
#endif

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
void StartRenderService();

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

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

	// The render texture which we use to render
	GLuint 							renderTexture;

	// The depth buffer of the render texture
	GLuint							depthBuffer;

	// The frame buffer to bind as render target
	GLuint							frameBuffer;

	// The render texture's width
	int								renderTextureWidth;

	// The render texture's height
	int								renderTextureHeight;

	// The pixel data of the render texture
	std::shared_ptr<GLubyte>		colorBuffer;

	// Used for FPS limiter.
	ULONGLONG						tick;

	// The starting time.
	ULONGLONG						startTick;
};

CubeRenderer*						g_cubeRenderer = nullptr;
std::map<int, std::shared_ptr<RemotePeerData>> g_remotePeersData;

void InitializeOpenGL(HWND handle)
{
	// Enables OpenGL support in window.
	PIXELFORMATDESCRIPTOR pfd = { 0 };
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;

	HDC hDC = GetDC(handle);
	int pf = ChoosePixelFormat(hDC, &pfd);
	if (pf == 0) 
	{
		MessageBox(NULL, L"ChoosePixelFormat() failed:  "
			"Cannot find a suitable pixel format.", L"Error", MB_OK);
	}

	if (SetPixelFormat(hDC, pf, &pfd) == FALSE) 
	{
		MessageBox(NULL, L"SetPixelFormat() failed:  "
			"Cannot set format specified.", L"Error", MB_OK);
	}

	DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	// Creates the rendering context.
	HGLRC hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);
	ReleaseDC(handle, hDC);

	// Initializes GLEW.
	if (glewInit() != GLEW_OK)
	{
		MessageBox(NULL, L"Failed to initialize GLEW", L"Error", MB_OK);
	}

	// Disables V-sync.
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	if (wglSwapIntervalEXT && wglGetSwapIntervalEXT)
	{
		wglSwapIntervalEXT(0);
	}

	// Initializes GL features.
	glShadeModel(GL_SMOOTH);                    // shading mathod: GL_SMOOTH or GL_FLAT
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
}

void InitializeRenderBuffer(RemotePeerData* peerData, int width, int height, bool isStereo)
{
	// Creates the render texture.
	glGenTextures(1, &peerData->renderTexture);
	glBindTexture(GL_TEXTURE_2D, peerData->renderTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, isStereo ? width << 1 : width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Creates the depth buffer.
	glGenRenderbuffers(1, &peerData->depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, peerData->depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, isStereo ? width << 1 : width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, peerData->depthBuffer);

	// Creates and binds the frame buffer.
	glGenFramebuffers(1, &peerData->frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, peerData->frameBuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, peerData->renderTexture, 0);
	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	// Stores the render texture's dimension.
	peerData->renderTextureWidth = width;
	peerData->renderTextureHeight = height;

	// Always check that our frame buffer is ok.
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		MessageBox(
			NULL,
			L"Failed to initialize the frame buffer.",
			L"Error",
			MB_ICONERROR
		);
	}

	// Creates the color buffer.
	peerData->colorBuffer.reset(new GLubyte[width * height * 4]);
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

	// Initializes OpenGL environment.
	InitializeOpenGL(wnd.handle());

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(fullServerConfig->server_config->server_config.width,
		fullServerConfig->server_config->server_config.height);
	
	// Initializes SSL.
	rtc::InitializeSSL();

	// Initializes the conductor.
	OpenGLMultiPeerConductor cond(fullServerConfig);

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
				InitializeRenderBuffer(
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
						InitializeRenderBuffer(
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
					if (!peerData->isStereo)
					{
						// FPS limiter.
						const int interval = 1000 / nvEncConfig->capture_fps;
						ULONGLONG timeElapsed = GetTickCount64() - peerData->tick;
						if (timeElapsed >= interval)
						{
							peerData->tick = GetTickCount64() - timeElapsed + interval;

							// Updates camera based on remote peer's input data.
							g_cubeRenderer->UpdateView(
								peerData->eyeVector,
								peerData->lookAtVector,
								peerData->upVector);

							// Main render.
							glClearColor(0.0, 0.0, 0.0, 0.0);
							glClearDepth(1.0f);
							glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							g_cubeRenderer->SetCamera();
							glPushMatrix();
							g_cubeRenderer->Render();
							glPopMatrix();
							g_cubeRenderer->ToPerspective();
							glRasterPos2i(0, 0);

							// Reads frame buffer.
							glReadPixels(
								0,
								0,
								peerData->renderTextureWidth,
								peerData->renderTextureHeight,
								GL_RGBA, GL_UNSIGNED_BYTE,
								peerData->colorBuffer.get());

							// Sends frame.
							peer->SendFrame(
								peerData->colorBuffer.get(),
								peerData->renderTextureWidth,
								peerData->renderTextureHeight);
						}
					}
				}
			}
		}
	}

	// Cleanup.
	for each (auto pair in g_remotePeersData)
	{
		RemotePeerData* peerData = pair.second.get();
		glDeleteTextures(1, &peerData->renderTexture);
		glDeleteRenderbuffers(1, &peerData->depthBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffersEXT(1, &peerData->frameBuffer);
	}

	rtc::CleanupSSL();
	delete g_cubeRenderer;

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
}
