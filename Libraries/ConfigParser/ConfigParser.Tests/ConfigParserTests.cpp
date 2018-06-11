#include "stdafx.h"
#include "CppUnitTest.h"
#include <string>

#define WIN32_LEAN_AND_MEAN // exclude rarely used windows content
#include <Windows.h>

#include "config_parser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace CppFactory;
using namespace StreamingToolkit;

namespace ConfigParserTests
{		
	TEST_CLASS(ConfigParserTests)
	{
	public:
		
		TEST_METHOD(Config_Get_Absolute_Path_Success)
		{
			// this logic is the same as in the method we're testing, which isn't ideal
			// but it's still worth testing because we want to see with and without filename
			// resolution work as expected and we just use this to compare against
			TCHAR buffer[MAX_PATH];
			GetModuleFileName(NULL, buffer, MAX_PATH);
			std::string::size_type pos = std::string(buffer).find_last_of("\\/");
			auto wrappedBuffer = std::string(buffer).substr(0, pos + 1);

			auto withoutFileName = ConfigParser::GetAbsolutePath("");
			auto withFileName = ConfigParser::GetAbsolutePath("file.txt");

			// ensure that resolution is working as we'd expect
			Assert::AreEqual(wrappedBuffer.c_str(), withoutFileName.c_str());
			Assert::AreEqual((wrappedBuffer + "file.txt").c_str(), withFileName.c_str());
		}

		TEST_METHOD(Config_Global_Allocation_Success)
		{
			// get our directory path
			TCHAR currentDirectory[MAX_PATH];
			GetModuleFileName(GetModuleHandle("ConfigParser.Tests.dll"), currentDirectory, MAX_PATH);
			auto wrappedCurrentDirectory = std::string(currentDirectory);

			// should configure the allocators for the default zones
			// note: we use our current directory (carefully removing our dll name from it) as the baseFilePath
			ConfigParser::ConfigureConfigFactories(wrappedCurrentDirectory.substr(0, wrappedCurrentDirectory.length() - 22));

			// should get an instance from the default zone
			auto injectedWebRTCInstance = Object<WebRTCConfig>::Get();

			// should get an instance from zone 1
			auto defaultWebRTCInstance = Object<WebRTCConfig>::Get<1>();

			// should be parsed from disk (see webrtcConfig.json in the test directory)
			Assert::AreEqual("test", injectedWebRTCInstance->ice_configuration.c_str());
			Assert::AreEqual("test:test:1234", injectedWebRTCInstance->turn_server.uri.c_str());
			Assert::AreEqual("test", injectedWebRTCInstance->turn_server.username.c_str());
			Assert::AreEqual("test", injectedWebRTCInstance->turn_server.password.c_str());
			Assert::AreEqual("testUri", injectedWebRTCInstance->server_uri.c_str());
			Assert::IsTrue(((uint16_t)5678) == injectedWebRTCInstance->port);
			Assert::IsTrue(((uint32_t)91011) == injectedWebRTCInstance->heartbeat);
			Assert::AreEqual("test:test:1234", injectedWebRTCInstance->stun_server.uri.c_str());
			Assert::AreEqual("testUri://testUri", injectedWebRTCInstance->authentication.authority_uri.c_str());
			Assert::AreEqual("00000000-0000-0000-0000-000000000000", injectedWebRTCInstance->authentication.client_id.c_str());
			Assert::AreEqual("test", injectedWebRTCInstance->authentication.client_secret.c_str());
			Assert::AreEqual("test://test", injectedWebRTCInstance->authentication.code_uri.c_str());
			Assert::AreEqual("test://test", injectedWebRTCInstance->authentication.poll_uri.c_str());
			Assert::AreEqual("00000000-0000-0000-0000-000000000000", injectedWebRTCInstance->authentication.resource.c_str());

			//// should be default initialized
			Assert::AreEqual("", defaultWebRTCInstance->ice_configuration.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->turn_server.uri.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->turn_server.username.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->turn_server.password.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->server_uri.c_str());
			Assert::IsTrue(((uint16_t)0) == defaultWebRTCInstance->port);
			Assert::IsTrue(((uint32_t)0) == defaultWebRTCInstance->heartbeat);
			Assert::AreEqual("", defaultWebRTCInstance->stun_server.uri.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.authority_uri.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.client_id.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.client_secret.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.code_uri.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.poll_uri.c_str());
			Assert::AreEqual("", defaultWebRTCInstance->authentication.resource.c_str());

			// should get an instance from the default zone
			auto injectedServerInstance = Object<ServerConfig>::Get();

			// should get an instance from zone 1
			auto defaultServerInstance = Object<ServerConfig>::Get<1>();

			// should be parsed from disk (see serverConfig.json in the test directory)
			Assert::IsTrue(((uint32_t)1234) == injectedServerInstance->server_config.height);
			Assert::AreEqual(true, injectedServerInstance->server_config.system_service);
			Assert::IsTrue(((uint32_t)5678) == injectedServerInstance->server_config.width);
			Assert::AreEqual(L"test", injectedServerInstance->service_config.display_name.c_str());
			Assert::AreEqual(L"test", injectedServerInstance->service_config.name.c_str());
			Assert::AreEqual(L"test\\test", injectedServerInstance->service_config.service_account.c_str());
			Assert::AreEqual(L"test", injectedServerInstance->service_config.service_password.c_str());

			// should be default initialized
			Assert::IsTrue(((uint32_t)0) == defaultServerInstance->server_config.height);
			Assert::AreEqual(false, defaultServerInstance->server_config.system_service);
			Assert::IsTrue(((uint32_t)0) == defaultServerInstance->server_config.width);
			Assert::AreEqual(L"", defaultServerInstance->service_config.display_name.c_str());
			Assert::AreEqual(L"", defaultServerInstance->service_config.name.c_str());
			Assert::AreEqual(L"", defaultServerInstance->service_config.service_account.c_str());
			Assert::AreEqual(L"", defaultServerInstance->service_config.service_password.c_str());
		}

		TEST_METHOD(Config_URI_Priority_Success)
		{
			// get our directory path
			TCHAR currentDirectory[MAX_PATH];
			GetModuleFileName(GetModuleHandle("ConfigParser.Tests.dll"), currentDirectory, MAX_PATH);
			auto wrappedCurrentDirectory = std::string(currentDirectory);

			// should configure the allocators for the default zones
			// note: we use our current directory (carefully removing our dll name from it) as the baseFilePath
			ConfigParser::ConfigureConfigFactories(wrappedCurrentDirectory.substr(0, wrappedCurrentDirectory.length() - 22), "dualWebrtcConfig.json");

			// should get an instance from the default zone
			auto dualWebRTCInstance = Object<WebRTCConfig>::Get();

			// ensure that the Uri postfixes have priority
			// No need to check that old behavior functions, as this is covered by previous test cases
			Assert::AreEqual("testUri", dualWebRTCInstance->server_uri.c_str());
			Assert::AreEqual("testUri://testUri", dualWebRTCInstance->authentication.authority_uri.c_str());

			ConfigParser::ConfigureConfigFactories(wrappedCurrentDirectory.substr(0, wrappedCurrentDirectory.length() - 22), "oldWebrtcConfig.json");

			auto oldWebRTCInstance = Object<WebRTCConfig>::Get();

			//ensure backwards compatibility for non-URI postfixes
			Assert::AreEqual("test", oldWebRTCInstance->server_uri.c_str());
			Assert::AreEqual("test://test", oldWebRTCInstance->authentication.authority_uri.c_str());
		}
	};
}