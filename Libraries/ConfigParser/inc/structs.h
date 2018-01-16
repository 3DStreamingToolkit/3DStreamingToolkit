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
	 * Video encoder configuration
	 */
	typedef struct
	{
		/* Enabling software encoding by default		*/
		bool			use_software_encoding;

		/* Capture frame rate							*/
		uint32_t		capture_fps;
	} NvEncConfig;
}
