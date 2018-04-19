#pragma once

#include <stdint.h>
#include <string>

namespace StreamingToolkit
{
	/*
	 * Server app configuration
	 */
	typedef struct
	{
		/* The video frame width						*/
		uint32_t		width;

		/* The video frame height						*/
		uint32_t		height;

		/* Running the app as a system service			*/
		bool			system_service;

		/* Running the system with max number of peers	*/
		int				system_capacity;

		/* Automatically calls the first connected peer	*/
		bool			auto_call;

		/* Automatically onnect to the signaling server	*/
		bool			auto_connect;
	} ServerAppConfig;

	/*
	 * System service configuration
	 */
	typedef struct
	{
		/* The system service's name					*/
		std::wstring	name;

		/* The system service's display name			*/
		std::wstring	display_name;

		/* The service account							*/
		std::wstring	service_account;

		/* The service password							*/
		std::wstring	service_password;
	} ServiceConfig;

	/*
	 * Server options configuration wrapper
	 */
	typedef struct
	{
		/* App config for server apps					*/
		ServerAppConfig server_config;

		/* service config for running as a service      */
		ServiceConfig service_config;
	} ServerConfig;

	/*
	 * Turn server configuration
	 */
	typedef struct
	{
		/* The turn server uri							*/
		std::string		uri;

		/* The turn credential uri						*/
		std::string		provider;

		/* The username used for authentication			*/
		std::string		username;

		/* The password used for authentication			*/
		std::string		password;
	} TurnServer;

	/*
	 * Stun server configuration
	 */
	typedef struct
	{
		/* The stun server uri							*/
		std::string		uri;
	} StunServer;

	/*
	 * Authentication configuration
	 */
	typedef struct
	{
		/* The uri used for token retrieval				*/
		std::string		authority;

		/*  OAuth 2.0 client credentials				*/
		std::string		resource;

		/*  OAuth 2.0 client credentials				*/
		std::string		client_id;

		/*  OAuth 2.0 client credentials				*/
		std::string		client_secret;

		/*  The uri used for auth code initiation		*/
		std::string		code_uri;

		/*  The uri used for auth code polling			*/
		std::string		poll_uri;
	} Authentication;

	/*
	 * webrtc configuration
	 */
	typedef struct
	{
		/* The ICE configuration: relay or all			*/
		std::string		ice_configuration;

		/* The turn server info							*/
		TurnServer		turn_server;

		/* The stun server info							*/
		StunServer		stun_server;

		/* The signaling server uri						*/
		std::string		server;

		/* The signaling server port					*/
		uint16_t		port;

		/* The heartbeat used to keep the app alive		*/
		uint32_t		heartbeat;

		/* The authentication info						*/
		Authentication	authentication;
	} WebRTCConfig;

	/*
	 * Full server configuration, including ServerConfig and WebRTCConfig
	 */
	typedef struct
	{
		std::shared_ptr<ServerConfig> server_config;
		std::shared_ptr<WebRTCConfig> webrtc_config;
	} FullServerConfig;

	/*
	 * Video encoder configuration
	 */
	typedef struct
	{
		/* Capture frame rate							*/
		uint32_t		capture_fps;
	} NvEncConfig;
}
