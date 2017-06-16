#include "pch.h"

#include <string>
#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "webrtc.h"
#include "third_party/jsoncpp/source/include/json/json.h"

//--------------------------------------------------------------------------------------
// Required app libs
//--------------------------------------------------------------------------------------
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

//--------------------------------------------------------------------------------------
// Global Methods
//--------------------------------------------------------------------------------------
std::string GetAbsolutePath(std::string fileName)
{
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	char charPath[MAX_PATH];
	wcstombs(charPath, buffer, wcslen(buffer) + 1);

	std::string::size_type pos = std::string(charPath).find_last_of("\\/");
	return std::string(charPath).substr(0, pos + 1) + fileName;
}

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------
int InitWebRTC(char* server, int port, std::string proxy)
{
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

	DefaultMainWindow wnd(server, port, FLAG_autoconnect, FLAG_autocall, false, 1280, 720);

	if (!wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	rtc::InitializeSSL();
	PeerConnectionClient client;
	client.SetProxy(proxy);

	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(&client, &wnd));

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

	rtc::CleanupSSL();

	return 0;
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

	int nArgs;
	char server[1024];
	strcpy(server, FLAG_server);
	int port = FLAG_port;
    char proxy[1024];
    strcpy(proxy, FLAG_proxy);
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

	// Try parsing command line arguments.
	if (szArglist && nArgs == 2)
	{
		wcstombs(server, szArglist[0], sizeof(server));
		port = _wtoi(szArglist[1]);
	}
    else if (szArglist && nArgs == 3)
    {
        wcstombs(server, szArglist[0], sizeof(server));
        port = _wtoi(szArglist[1]);
        wcstombs(proxy, szArglist[2], sizeof(proxy));
    }
	else // Try parsing config file.
	{
		std::string configFilePath = GetAbsolutePath("webrtcConfig.json");
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

			if (root.isMember("proxy"))
			{
				strcpy(proxy, root.get("proxy", FLAG_proxy).asCString());
			}
		}
	}

	return InitWebRTC(server, port, proxy);
}
