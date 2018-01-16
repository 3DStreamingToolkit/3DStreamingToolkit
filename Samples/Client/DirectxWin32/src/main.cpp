#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>
#include <functional>
#include <algorithm>

#include "webrtc.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "client_main_window.h"
#include "win32_data_channel_handler.h"
#include "oauth24d_provider.h"
#include "turn_credential_provider.h"
#include "config_parser.h"

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

using namespace StreamingToolkit;

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

	// this must occur before any config access
	ConfigParser::ConfigureConfigFactories();

	// get and keep a reference to webrtc config, we're going to use it alot below
	auto webrtcConfig = GlobalObject<WebRTCConfig>::Get();

	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

	ClientMainWindow wnd(
		webrtcConfig->server.c_str(),
		webrtcConfig->port,
		FLAG_autoconnect,
		FLAG_autocall,
		false,
		1280,
		720);

	if (!wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	rtc::InitializeSSL();

	std::unique_ptr<OAuth24DProvider> oauth;
	if (!webrtcConfig->authentication.code_uri.empty() &&
		!webrtcConfig->authentication.poll_uri.empty())
	{
		oauth.reset(new OAuth24DProvider(
			webrtcConfig->authentication.code_uri, webrtcConfig->authentication.poll_uri));
	}
	else
	{
		wnd.SetAuthCode(L"Not configured");
		wnd.SetAuthUri(L"Not configured");
	}

	// depends on oauth
	std::unique_ptr<TurnCredentialProvider> turn;

	if (oauth.get() != nullptr &&
		webrtcConfig->turn_server.provider != FLAG_turnUri &&
		!webrtcConfig->turn_server.provider.empty())
	{
		turn.reset(new TurnCredentialProvider(webrtcConfig->turn_server.provider));
	}

	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(&client, &wnd, webrtcConfig.get()));

	Win32DataChannelHandler dcHandler(conductor.get());

	wnd.SignalClientWindowMessage.connect(&dcHandler, &Win32DataChannelHandler::ProcessMessage);
	wnd.SignalDataChannelMessage.connect(&dcHandler, &Win32DataChannelHandler::ProcessMessage);

	// set our client heartbeat interval
	client.SetHeartbeatMs(webrtcConfig->heartbeat);

	// create (but not necessarily use) async callbacks
	TurnCredentialProvider::CredentialsRetrievedCallback credentialsRetrieved([&](const TurnCredentials& data)
	{
		if (data.successFlag)
		{
			conductor->SetTurnCredentials(data.username, data.password);

			wnd.SetConnectButtonState(true);

			// redraw the ui that shows the connect button only if we're currently in that ui
			if (wnd.current_ui() == ClientMainWindow::UI::CONNECT_TO_SERVER)
			{
				wnd.SwitchToConnectUI();
			}
		}
		else
		{
			wnd.MessageBoxW("Turn Credentials", "Unable to retrieve turn creds", true);
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
		if (wnd.current_ui() == ClientMainWindow::UI::CONNECT_TO_SERVER)
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

			if (turn.get() == nullptr)
			{
				wnd.SetConnectButtonState(true);
			}

			// redraw the ui that shows the code only if we're currently in that ui
			if (wnd.current_ui() == ClientMainWindow::UI::CONNECT_TO_SERVER)
			{
				wnd.SwitchToConnectUI();
			}
		}
		else
		{
			wnd.MessageBoxW("Authentication", "Unable to complete authentication", true);
		}
	});

	// if we have real turn values, configure turn
	if (turn.get() != nullptr)
	{
		turn->SetAuthenticationProvider(oauth.get());

		turn->SignalCredentialsRetrieved.connect(&credentialsRetrieved, &TurnCredentialProvider::CredentialsRetrievedCallback::Handle);
	}

	// if we have real auth values, indicate that we'll try and connect
	if (oauth.get() != nullptr)
	{
		oauth->SignalCodeComplete.connect(&codeComplete, &OAuth24DProvider::CodeCompleteCallback::Handle);
		oauth->SignalAuthenticationComplete.connect(&authComplete, &AuthenticationProvider::AuthenticationCompleteCallback::Handle);

		wnd.SetConnectButtonState(false);
		wnd.SetAuthCode(L"Connecting");
		wnd.SetAuthUri(L"Connecting");

		// redraw the ui that shows the code only if we're currently in that ui
		if (wnd.current_ui() == ClientMainWindow::UI::CONNECT_TO_SERVER)
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
