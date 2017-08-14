#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>
#include <functional>
#include <algorithm>

#include "webrtc.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "oauth24d_provider.h"
#include "turn_credential_provider.h"

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
int InitWebRTC(char* server, int port, int heartbeat, char* authCodeUri, char* authPollUri, char* turnUri)
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

	std::unique_ptr<OAuth24DProvider> oauth;
	if (strcmp(authCodeUri, FLAG_authCodeUri) != 0 &&
		strcmp(authPollUri, FLAG_authPollUri) != 0 &&
		!std::string(authCodeUri).empty() &&
		!std::string(authPollUri).empty())
	{
		oauth.reset(new OAuth24DProvider(authCodeUri, authPollUri));
	}

	// depends on oauth
	std::unique_ptr<TurnCredentialProvider> turn;
	if (oauth.get() != nullptr &&
		strcmp(turnUri, FLAG_turnUri) != 0 &&
		!std::string(turnUri).empty())
	{
		turn.reset(new TurnCredentialProvider(turnUri));
	}

	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(&client, &wnd));

	// set our client heartbeat interval
	client.SetHeartbeatMs(heartbeat);

	// create (but not necessarily use) async callbacks
	TurnCredentialProvider::CredentialsRetrievedCallback credentialsRetrieved([&](const TurnCredentials& data)
	{
		if (data.successFlag)
		{
			conductor->SetTurnCredentials(data.username, data.password);

			wnd.SetConnectButtonState(true);

			// redraw the ui that shows the connect button only if we're currently in that ui
			if (wnd.current_ui() == DefaultMainWindow::UI::CONNECT_TO_SERVER)
			{
				wnd.SwitchToConnectUI();
			}
		}
	});
	OAuth24DProvider::CodeCompleteCallback codeComplete([&](const OAuth24DProvider::CodeData& data) {
		std::wstring wcode(data.user_code.begin(), data.user_code.end());
		std::wstring wuri(data.verification_url.begin(), data.verification_url.end());
		std::replace(wuri.begin(), wuri.end(), L'\\', L'/');

		// set the ui values
		wnd.SetAuthCode(wcode);
		wnd.SetAuthUri(wuri);

		// redraw the ui that shows the code only if we're currently in that ui
		if (wnd.current_ui() == DefaultMainWindow::UI::CONNECT_TO_SERVER)
		{
			wnd.SwitchToConnectUI();
		}
	});
	AuthenticationProvider::AuthenticationCompleteCallback authComplete([&](const AuthenticationProviderResult& data) {
		if (data.successFlag)
		{
			client.SetAuthorizationHeader("Bearer " + data.accessToken);

			// let the user know auth is complete
			wnd.SetAuthCode(L"OK");
			wnd.SetAuthUri(L"Authenticated");

			// redraw the ui that shows the code only if we're currently in that ui
			if (wnd.current_ui() == DefaultMainWindow::UI::CONNECT_TO_SERVER)
			{
				wnd.SwitchToConnectUI();
			}
		}
	});

	// if we have real turn values, configure turn
	if (turn.get() != nullptr)
	{
		// disable the connect button until turn is resolved
		wnd.SetConnectButtonState(false);

		// redraw the ui that shows the code only if we're currently in that ui
		if (wnd.current_ui() == DefaultMainWindow::UI::CONNECT_TO_SERVER)
		{
			wnd.SwitchToConnectUI();
		}

		turn->SetAuthenticationProvider(oauth.get());

		turn->SignalCredentialsRetrieved.connect(&credentialsRetrieved, &TurnCredentialProvider::CredentialsRetrievedCallback::Handle);
	}

	// if we have real auth values, indicate that we'll try and connect
	if (oauth.get() != nullptr)
	{
		
		oauth->SignalCodeComplete.connect(&codeComplete, &OAuth24DProvider::CodeCompleteCallback::Handle);
		oauth->SignalAuthenticationComplete.connect(&authComplete, &AuthenticationProvider::AuthenticationCompleteCallback::Handle);

		wnd.SetAuthCode(L"Connecting");
		wnd.SetAuthUri(L"Connecting");
		
		// redraw the ui that shows the code only if we're currently in that ui
		if (wnd.current_ui() == DefaultMainWindow::UI::CONNECT_TO_SERVER)
		{
			wnd.SwitchToConnectUI();
		}

		// do auth things
		if (turn.get() != nullptr)
		{
			// this will trigger oauth->Authenticate() under the hood
			if (!turn->RequestCredentials())
			{
				wnd.SetAuthCode(L"FAIL");
				wnd.SetAuthUri(L"Unable to request turn creds");
			}
		}
		// if we don't have a turn provider, we just authenticate
		else if (!oauth->Authenticate())
		{
			wnd.SetAuthCode(L"FAIL");
			wnd.SetAuthUri(L"Unable to authenticate");
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
	char authCodeUri[1024];
	strcpy(authCodeUri, FLAG_authCodeUri);
	char authPollUri[1024];
	strcpy(authPollUri, FLAG_authPollUri);
	char turnUri[1024];
	strcpy(turnUri, FLAG_turnUri);
	int port = FLAG_port;
	int heartbeat = FLAG_heartbeat;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

	// Try parsing command line arguments.
	if (szArglist && nArgs == 2)
	{
		wcstombs(server, szArglist[0], sizeof(server));
		port = _wtoi(szArglist[1]);
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

			if (root.isMember("heartbeat"))
			{
				heartbeat = root.get("heartbeat", FLAG_heartbeat).asInt();
			}

			if (root.isMember("turnServer"))
			{
				auto turnWrapper = root.get("turnServer", NULL);
				if (turnWrapper != NULL)
				{
					if (turnWrapper.isMember("provider"))
					{
						strcpy(turnUri, turnWrapper.get("provider", FLAG_turnUri).asCString());
					}
				}
			}

			if (root.isMember("authentication"))
			{
				auto authenticationWrapper = root.get("authentication", NULL);
				if (authenticationWrapper != NULL)
				{
					if (authenticationWrapper.isMember("codeUri"))
					{
						strcpy(authCodeUri, authenticationWrapper.get("codeUri", FLAG_authCodeUri).asCString());
					}

					if (authenticationWrapper.isMember("pollUri"))
					{
						strcpy(authPollUri, authenticationWrapper.get("pollUri", FLAG_authPollUri).asCString());
					}
				}
			}
		}
	}

	return InitWebRTC(server, port, heartbeat, authCodeUri, authPollUri, turnUri);
}
