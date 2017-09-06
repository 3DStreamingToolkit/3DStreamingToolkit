#include "pch.h"

#include <fstream>
#include "config_parser.h"
#include "third_party/jsoncpp/source/include/json/json.h"

using namespace StreamingToolkit;

std::string ConfigParser::GetAbsolutePath(const std::string& file_name)
{
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	char charPath[MAX_PATH];
	wcstombs(charPath, buffer, wcslen(buffer) + 1);

	std::string::size_type pos = std::string(charPath).find_last_of("\\/");
	return std::string(charPath).substr(0, pos + 1) + file_name;
}

void ConfigParser::Parse(
	const std::string& file_name,
	ServerConfig* server_config,
	ServiceConfig* service_config)
{
	memset(server_config, sizeof(ServerConfig), 0);
	memset(service_config, sizeof(ServiceConfig), 0);
	std::string file = GetAbsolutePath(file_name);
	std::ifstream fileStream(file);
	Json::Reader reader;
	Json::Value root = NULL;
	if (fileStream.good())
	{
		reader.parse(fileStream, root, true);
		if (root.isMember("serverConfig"))
		{
			auto serverConfigNode = root.get("serverConfig", NULL);
			if (serverConfigNode.isMember("width"))
			{
				server_config->width = serverConfigNode.get("width", "").asInt();
			}

			if (serverConfigNode.isMember("height"))
			{
				server_config->height = serverConfigNode.get("height", "").asInt();
			}

			if (serverConfigNode.isMember("systemService"))
			{
				server_config->system_service = serverConfigNode.get("systemService", "").asBool();
			}
		}

		if (root.isMember("serviceConfig"))
		{
			auto serviceConfigNode = root.get("serviceConfig", NULL);
			if (serviceConfigNode.isMember("name"))
			{
				std::string name = serviceConfigNode.get("name", "").asString();
				service_config->name.assign(name.begin(), name.end());
			}

			if (serviceConfigNode.isMember("displayName"))
			{
				std::string displayName = serviceConfigNode.get("displayName", "").asString();
				service_config->display_name.assign(displayName.begin(), displayName.end());
			}

			if (serviceConfigNode.isMember("serviceAccount"))
			{
				std::string serviceAccount = serviceConfigNode.get("serviceAccount", "").asString();
				service_config->service_account.assign(serviceAccount.begin(), serviceAccount.end());
			}

			if (serviceConfigNode.isMember("servicePassword"))
			{
				std::string servicePassword = serviceConfigNode.get("servicePassword", "").asString();
				service_config->service_password.assign(servicePassword.begin(), servicePassword.end());
			}
		}
	}
}

void ConfigParser::Parse(
	const std::string& file_name,
	WebRTCConfig* webrtc_config)
{
	memset(webrtc_config, sizeof(WebRTCConfig), 0);
	std::string file = GetAbsolutePath(file_name);
	std::ifstream fileStream(file);
	Json::Reader reader;
	Json::Value root = NULL;
	if (fileStream.good())
	{
		reader.parse(fileStream, root, true);
		webrtc_config->ice_configuration = root.get("iceConfiguration", NULL).asString();
		if (root.isMember("turnServer"))
		{
			auto turnServerNode = root.get("turnServer", NULL);
			if (turnServerNode.isMember("uri"))
			{
				webrtc_config->turn_server.uri = turnServerNode.get("uri", NULL).asString();
			}

			if (turnServerNode.isMember("provider"))
			{
				webrtc_config->turn_server.provider = turnServerNode.get("provider", NULL).asString();
			}

			if (turnServerNode.isMember("username"))
			{
				webrtc_config->turn_server.username = turnServerNode.get("username", NULL).asString();
			}

			if (turnServerNode.isMember("password"))
			{
				webrtc_config->turn_server.password = turnServerNode.get("password", NULL).asString();
			}
		}

		if (root.isMember("server"))
		{
			webrtc_config->server = root.get("server", NULL).asString();
		}

		if (root.isMember("port"))
		{
			webrtc_config->port = root.get("port", NULL).asInt();
		}

		if (root.isMember("heartbeat"))
		{
			webrtc_config->heartbeat = root.get("heartbeat", NULL).asInt();
		}

		if (root.isMember("authentication"))
		{
			auto authenticationNode = root.get("authentication", NULL);
			if (authenticationNode.isMember("authority"))
			{
				webrtc_config->authentication.authority = authenticationNode.get("authority", NULL).asString();
			}

			if (authenticationNode.isMember("resource"))
			{
				webrtc_config->authentication.resource = authenticationNode.get("resource", NULL).asString();
			}

			if (authenticationNode.isMember("clientId"))
			{
				webrtc_config->authentication.client_id = authenticationNode.get("clientId", NULL).asString();
			}

			if (authenticationNode.isMember("clientSecret"))
			{
				webrtc_config->authentication.client_secret = authenticationNode.get("clientSecret", NULL).asString();
			}
		}
	}
}
