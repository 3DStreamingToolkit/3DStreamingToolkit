#include "config_parser.h"

#include <fstream>

#define WIN32_LEAN_AND_MEAN // exclude rarely used windows content
#include <windows.h>

#include <third_party/jsoncpp/source/include/json/json.h>

using namespace StreamingToolkit;

const char* ConfigParser::kWebrtcConfigPath = "webrtcConfig.json";
const char* ConfigParser::kServerConfigPath = "serverConfig.json";
const char* ConfigParser::kNvEncConfigPath = "nvEncConfig.json";

std::string ConfigParser::GetAbsolutePath(const std::string& file_name)
{
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos + 1) + file_name;
}

void ConfigParser::ConfigureConfigFactories()
{
	ConfigureConfigFactories(GetAbsolutePath(""));
}

void ConfigParser::ConfigureConfigFactories(const std::string& baseFilePath)
{
	CppFactory::Object<StreamingToolkit::WebRTCConfig>::RegisterAllocator([=] {
		auto config = new StreamingToolkit::WebRTCConfig();

		ConfigParser::ParseWebRTCConfig(baseFilePath + std::string(ConfigParser::kWebrtcConfigPath), config);

		return std::shared_ptr<StreamingToolkit::WebRTCConfig>(config);
	});

	CppFactory::Object<StreamingToolkit::ServerConfig>::RegisterAllocator([=] {
		auto config = new StreamingToolkit::ServerConfig();

		ConfigParser::ParseServerConfig(baseFilePath + std::string(ConfigParser::kServerConfigPath), config);

		return std::shared_ptr<StreamingToolkit::ServerConfig>(config);
	});

	CppFactory::Object<StreamingToolkit::NvEncConfig>::RegisterAllocator([=] {
		auto config = new StreamingToolkit::NvEncConfig();

		ConfigParser::ParseNvEncConfig(baseFilePath + std::string(ConfigParser::kNvEncConfigPath), config);

		return std::shared_ptr<StreamingToolkit::NvEncConfig>(config);
	});
}

void ConfigParser::ParseWebRTCConfig(const std::string& path, StreamingToolkit::WebRTCConfig* webrtcConfig)
{
	std::ifstream fileStream(path);
	Json::Reader reader;
	Json::Value root = NULL;
	if (fileStream.good())
	{
		reader.parse(fileStream, root, true);
		webrtcConfig->ice_configuration = root.get("iceConfiguration", NULL).asString();
		if (root.isMember("turnServer"))
		{
			auto turnServerNode = root.get("turnServer", NULL);
			if (turnServerNode.isMember("uri"))
			{
				webrtcConfig->turn_server.uri = turnServerNode.get("uri", NULL).asString();
			}

			if (turnServerNode.isMember("provider"))
			{
				webrtcConfig->turn_server.provider = turnServerNode.get("provider", NULL).asString();
			}

			if (turnServerNode.isMember("username"))
			{
				webrtcConfig->turn_server.username = turnServerNode.get("username", NULL).asString();
			}

			if (turnServerNode.isMember("password"))
			{
				webrtcConfig->turn_server.password = turnServerNode.get("password", NULL).asString();
			}
		}

		if (root.isMember("stunServer"))
		{
			auto stunServerNode = root.get("stunServer", NULL);
			if (stunServerNode.isMember("uri"))
			{
				webrtcConfig->stun_server.uri = stunServerNode.get("uri", NULL).asString();
			}
		}

		if (root.isMember("server"))
		{
			webrtcConfig->server = root.get("server", NULL).asString();
		}

		if (root.isMember("port"))
		{
			webrtcConfig->port = root.get("port", NULL).asInt();
		}

		if (root.isMember("heartbeat"))
		{
			webrtcConfig->heartbeat = root.get("heartbeat", NULL).asInt();
		}

		if (root.isMember("authentication"))
		{
			auto authenticationNode = root.get("authentication", NULL);
			if (authenticationNode.isMember("authority"))
			{
				webrtcConfig->authentication.authority = authenticationNode.get("authority", NULL).asString();
			}

			if (authenticationNode.isMember("resource"))
			{
				webrtcConfig->authentication.resource = authenticationNode.get("resource", NULL).asString();
			}

			if (authenticationNode.isMember("clientId"))
			{
				webrtcConfig->authentication.client_id = authenticationNode.get("clientId", NULL).asString();
			}

			if (authenticationNode.isMember("clientSecret"))
			{
				webrtcConfig->authentication.client_secret = authenticationNode.get("clientSecret", NULL).asString();
			}

			if (authenticationNode.isMember("codeUri"))
			{
				webrtcConfig->authentication.code_uri = authenticationNode.get("codeUri", NULL).asString();
			}

			if (authenticationNode.isMember("pollUri"))
			{
				webrtcConfig->authentication.poll_uri = authenticationNode.get("pollUri", NULL).asString();
			}
		}
	}
}

void ConfigParser::ParseServerConfig(const std::string& path, StreamingToolkit::ServerConfig* serverConfig)
{
	std::ifstream fileStream(path);
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
				serverConfig->server_config.width = serverConfigNode.get("width", "").asInt();
			}

			if (serverConfigNode.isMember("height"))
			{
				serverConfig->server_config.height = serverConfigNode.get("height", "").asInt();
			}

			if (serverConfigNode.isMember("systemService"))
			{
				serverConfig->server_config.system_service = serverConfigNode.get("systemService", "").asBool();
			}
		}

		if (root.isMember("serviceConfig"))
		{
			auto serviceConfigNode = root.get("serviceConfig", NULL);
			if (serviceConfigNode.isMember("name"))
			{
				std::string name = serviceConfigNode.get("name", "").asString();
				serverConfig->service_config.name.assign(name.begin(), name.end());
			}

			if (serviceConfigNode.isMember("displayName"))
			{
				std::string displayName = serviceConfigNode.get("displayName", "").asString();
				serverConfig->service_config.display_name.assign(displayName.begin(), displayName.end());
			}

			if (serviceConfigNode.isMember("serviceAccount"))
			{
				std::string serviceAccount = serviceConfigNode.get("serviceAccount", "").asString();
				serverConfig->service_config.service_account.assign(serviceAccount.begin(), serviceAccount.end());
			}

			if (serviceConfigNode.isMember("servicePassword"))
			{
				std::string servicePassword = serviceConfigNode.get("servicePassword", "").asString();
				serverConfig->service_config.service_password.assign(servicePassword.begin(), servicePassword.end());
			}
		}
	}
}

void ConfigParser::ParseNvEncConfig(const std::string& path, StreamingToolkit::NvEncConfig* nvEncConfig)
{
	std::ifstream fileStream(path);
	Json::Reader reader;
	Json::Value root = NULL;
	if (fileStream.good())
	{
		reader.parse(fileStream, root, true);
		if (root.isMember("useSoftwareEncoding"))
		{
			nvEncConfig->use_software_encoding = root.get("useSoftwareEncoding", NULL).asBool();
		}

		if (root.isMember("serverFrameCaptureFPS"))
		{
			nvEncConfig->capture_fps = root.get("serverFrameCaptureFPS", NULL).asInt();
		}
	}
}
