#pragma once

#include "structs.h"

namespace StreamingToolkit
{
	class ConfigParser
	{
	public:
		static std::string GetAbsolutePath(const std::string& file_name);

		static void Parse(
			const std::string& file_name,
			ServerConfig* server_config,
			ServiceConfig* service_config);

		static void Parse(const std::string& file_name, WebRTCConfig* webrtc_config);
	};
}
