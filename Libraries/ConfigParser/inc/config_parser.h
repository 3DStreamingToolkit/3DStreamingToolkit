#pragma once

#include <string>
#include <CppFactory.hpp>

#include "structs.h"

using namespace CppFactory;

namespace StreamingToolkit
{
	/// <summary>
	/// Configuration parser
	/// </summary>
	/// <remarks>
	/// It is expected that CppFactory Object or GlobalObject will be used to access
	/// configuration throughout the application, and that these access calls will
	/// occur in the default zone. see <see cref="ConfigureConfigFactories()"/>.
	/// </remarks>
	class ConfigParser
	{
	public:
		/// <summary>
		/// Represents the relative path at which we expect webrtcConfig to exist
		/// </summary>
		static const char* kWebrtcConfigPath;

		/// <summary>
		/// Represents the relative path at which we expect serverConfig to exist
		/// </summary>
		static const char* kServerConfigPath;

		/// <summary>
		/// Represents the relative path at which we expect nvEncConfig to exist
		/// </summary>
		static const char* kNvEncConfigPath;

		/// <summary>
		/// Configures CppFactory object support for our toplevel configuration models
		/// <see cref="WebRTCConfig"/> and <see cref="ServerConfig"/> respectively
		/// </summary>
		/// <remarks>
		/// Expected to be called on application start, as this enables our factory injection
		/// of Object&lt;WebRTCConfig&gt; Object&lt;ServerConfig&gt; to contain valid values
		/// read from disk on the first Get() call.
		/// 
		/// Note: this this configures injection for the default zone, as config objects
		/// are just representation of disk values, so there's no value in having unique
		/// instances of these types.
		/// </remarks>
		static void ConfigureConfigFactories();

		/// <summary>
		/// Configures CppFactory object support for our toplevel configuration models
		/// <see cref="WebRTCConfig"/> and <see cref="ServerConfig"/> respectively
		/// </summary>
		/// <remarks>
		/// Expected to be called on application start, as this enables our factory injection
		/// of Object&lt;WebRTCConfig&gt; Object&lt;ServerConfig&gt; to contain valid values
		/// read from disk on the first Get() call.
		/// 
		/// Note: this this configures injection for the default zone, as config objects
		/// are just representation of disk values, so there's no value in having unique
		/// instances of these types.
		/// </remarks>
		/// <param name="baseFilePath">a base path to look for configuration files in</param>
		static void ConfigureConfigFactories(const std::string& baseFilePath);

		/// <summary>
		/// Get the absolute path for a relative file
		/// </summary>
		/// <param name="file_name">the relative file</param>
		/// <returns>the absolute path</returns>
		static std::string ConfigParser::GetAbsolutePath(const std::string& file_name);

		ConfigParser() = delete;
		~ConfigParser() = delete;

	private:
		static void ParseWebRTCConfig(const std::string& path, StreamingToolkit::WebRTCConfig* webrtcConfig);
		static void ParseServerConfig(const std::string& path, StreamingToolkit::ServerConfig* serverConfig);
		static void ParseNvEncConfig(const std::string& path, StreamingToolkit::NvEncConfig* nvEncConfig);
	};
}