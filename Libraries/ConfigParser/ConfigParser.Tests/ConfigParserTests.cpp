#include <string>
#include <gtest\gtest.h>
#include <windows.h>
#include "config_parser.h"

#pragma comment(lib, "webrtc.lib")

using namespace CppFactory;
using namespace StreamingToolkit;

TEST(ConfigParserTests, Config_Get_Absolute_Path_Success)
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
	ASSERT_STREQ(wrappedBuffer.c_str(), withoutFileName.c_str());
	ASSERT_STREQ((wrappedBuffer + "file.txt").c_str(), withFileName.c_str());
}

TEST(ConfigParserTests, Config_Global_Allocation_Success)
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
	ASSERT_STREQ("test", injectedWebRTCInstance->ice_configuration.c_str());
	ASSERT_STREQ("test:test:1234", injectedWebRTCInstance->turn_server.uri.c_str());
	ASSERT_STREQ("test", injectedWebRTCInstance->turn_server.username.c_str());
	ASSERT_STREQ("test", injectedWebRTCInstance->turn_server.password.c_str());
	ASSERT_STREQ("testUri", injectedWebRTCInstance->server_uri.c_str());
	ASSERT_TRUE(((uint16_t)5678) == injectedWebRTCInstance->port);
	ASSERT_TRUE(((uint32_t)91011) == injectedWebRTCInstance->heartbeat);
	ASSERT_STREQ("test:test:1234", injectedWebRTCInstance->stun_server.uri.c_str());
	ASSERT_STREQ("testUri://testUri", injectedWebRTCInstance->authentication.authority_uri.c_str());
	ASSERT_STREQ("00000000-0000-0000-0000-000000000000", injectedWebRTCInstance->authentication.client_id.c_str());
	ASSERT_STREQ("test", injectedWebRTCInstance->authentication.client_secret.c_str());
	ASSERT_STREQ("test://test", injectedWebRTCInstance->authentication.code_uri.c_str());
	ASSERT_STREQ("test://test", injectedWebRTCInstance->authentication.poll_uri.c_str());
	ASSERT_STREQ("00000000-0000-0000-0000-000000000000", injectedWebRTCInstance->authentication.resource.c_str());

	//// should be default initialized
	ASSERT_STREQ("", defaultWebRTCInstance->ice_configuration.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->turn_server.uri.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->turn_server.username.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->turn_server.password.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->server_uri.c_str());
	ASSERT_TRUE(((uint16_t)0) == defaultWebRTCInstance->port);
	ASSERT_TRUE(((uint32_t)0) == defaultWebRTCInstance->heartbeat);
	ASSERT_STREQ("", defaultWebRTCInstance->stun_server.uri.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.authority_uri.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.client_id.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.client_secret.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.code_uri.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.poll_uri.c_str());
	ASSERT_STREQ("", defaultWebRTCInstance->authentication.resource.c_str());

	// should get an instance from the default zone
	auto injectedServerInstance = Object<ServerConfig>::Get();

	// should get an instance from zone 1
	auto defaultServerInstance = Object<ServerConfig>::Get<1>();

	// should be parsed from disk (see serverConfig.json in the test directory)
	ASSERT_TRUE(((uint32_t)1234) == injectedServerInstance->server_config.height);
	ASSERT_EQ(true, injectedServerInstance->server_config.system_service);
	ASSERT_TRUE(((uint32_t)5678) == injectedServerInstance->server_config.width);
	ASSERT_STREQ(L"test", injectedServerInstance->service_config.display_name.c_str());
	ASSERT_STREQ(L"test", injectedServerInstance->service_config.name.c_str());
	ASSERT_STREQ(L"test\\test", injectedServerInstance->service_config.service_account.c_str());
	ASSERT_STREQ(L"test", injectedServerInstance->service_config.service_password.c_str());

	// should be default initialized
	ASSERT_TRUE(((uint32_t)0) == defaultServerInstance->server_config.height);
	ASSERT_EQ(false, defaultServerInstance->server_config.system_service);
	ASSERT_TRUE(((uint32_t)0) == defaultServerInstance->server_config.width);
	ASSERT_STREQ(L"", defaultServerInstance->service_config.display_name.c_str());
	ASSERT_STREQ(L"", defaultServerInstance->service_config.name.c_str());
	ASSERT_STREQ(L"", defaultServerInstance->service_config.service_account.c_str());
	ASSERT_STREQ(L"", defaultServerInstance->service_config.service_password.c_str());
}

TEST(ConfigParserTests, Config_URI_Priority_Success)
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
	ASSERT_STREQ("testUri", dualWebRTCInstance->server_uri.c_str());
	ASSERT_STREQ("testUri://testUri", dualWebRTCInstance->authentication.authority_uri.c_str());

	ConfigParser::ConfigureConfigFactories(wrappedCurrentDirectory.substr(0, wrappedCurrentDirectory.length() - 22), "oldWebrtcConfig.json");

	auto oldWebRTCInstance = Object<WebRTCConfig>::Get();

	//ensure backwards compatibility for non-URI postfixes
	ASSERT_STREQ("test", oldWebRTCInstance->server_uri.c_str());
	ASSERT_STREQ("test://test", oldWebRTCInstance->authentication.authority_uri.c_str());
}
