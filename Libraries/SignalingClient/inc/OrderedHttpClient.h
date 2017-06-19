#pragma once

#include <string>
#include <cpprest/http_client.h>

using namespace std;
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;

namespace SignalingClient
{
	namespace Http
	{
		/// <summary>
		/// An http client that ensures requests are sent in the order they are issued
		/// </summary>
		class OrderedHttpClient
		{
		public:
			/// <summary>
			/// Helper function to determine if a client is default initialized
			/// </summary>
			/// <param name="client">the client</param>
			/// <returns>a flag indicating if the client is default initialized</returns>
			static bool IsDefaultInitialized(const OrderedHttpClient& client);

			/// <summary>
			/// Default ctor
			/// </summary>
			OrderedHttpClient();

			/// <summary>
			/// Create a client with a given baseUrl
			/// </summary>
			/// <param name="baseUrl">the base url that you wish to issue requests to</param>
			OrderedHttpClient(const string& baseUrl);

			/// <summary>
			/// Create a client with a given baseUrl
			/// </summary>
			/// <param name="baseUrl">the base url that you wish to issue requests to</param>
			OrderedHttpClient(const string_t& baseUrl);

			/// <summary>
			/// Create a client with a given baseUrl and a given underlying configuration
			/// </summary>
			/// <param name="baseUrl">the base url that you wish to issue requests to</param>
			/// <param name="config">the underlying client configuration to use</param>
			OrderedHttpClient(const string_t& baseUrl, const http_client_config& config);

			/// <summary>
			/// Default dtor
			/// </summary>
			~OrderedHttpClient();

			/// <summary>
			/// Issue a request
			/// </summary>
			/// <param name="requestMethod">the http method to use</param>
			/// <param name="requestEndpoint">the endpoint url to use</param>
			/// <returns>a task that resolves when the response is received</returns>
			task<http_response> Issue(const method& requestMethod, const string& requestEndpoint);

			/// <summary>
			/// Issue a request
			/// </summary>
			/// <param name="requestMethod">the http method to use</param>
			/// <param name="requestEndpoint">the endpoint url to use</param>
			/// <param name="requestBody">the body content to use</param>
			/// <returns>a task that resolves when the response is received</returns>
			task<http_response> Issue(const method& requestMethod, const string& requestEndpoint, const string& requestBody);

			/// <summary>
			/// Issue a request
			/// </summary>
			/// <param name="requestMethod">the http method to use</param>
			/// <param name="requestEndpoint">the endpoint url to use</param>
			/// <returns>a task that resolves when the response is received</returns>
			task<http_response> Issue(const method& requestMethod, const string_t& requestEndpoint);

			/// <summary>
			/// Issue a request
			/// </summary>
			/// <param name="requestMethod">the http method to use</param>
			/// <param name="requestEndpoint">the endpoint url to use</param>
			/// <param name="requestBody">the body content to use</param>
			/// <returns>a task that resolves when the response is received</returns>
			task<http_response> Issue(const method& requestMethod, const string_t& requestEndpoint, const string_t& requestBody);

			/// <summary>
			/// Cancel all pending requests
			/// </summary>
			void CancelAll();

			/// <summary>
			/// Gets the underlying client config
			/// </summary>
			const http_client_config config();

			/// <summary>
			/// Sets the underlying client config
			/// </summary>
			/// <param name="config">new config</param>
			void SetConfig(const http_client_config& config);

			/// <summary>
			/// Gets the configured baseUrl
			/// </summary>
			const uri uri();

			/// <summary>
			/// Sets the baseUrl
			/// </summary>
			/// <param name="uri">new url</param>
			void SetUri(const web::uri& uri);

			/// <summary>
			/// Gets the most recently issued request's response task
			/// </summary>
			/// <remarks>
			/// This can be used to determine if all issued requests are complete
			/// </remarks>
			const task<http_response> most_recent();

		private:
			/// <summary>
			/// flag used to track if the default ctor was used
			/// </summary>
			/// <remarks>
			/// We track this because there are some common scenarios where
			/// we wish to know if the baseUrl and config are ready for use
			/// </remarks>
			bool default_init_;

			/// <summary>
			/// The baseUrl
			/// </summary>
			web::uri request_root_;

			/// <summary>
			/// The underlying client configuration
			/// </summary>
			http_client_config request_config_;

			/// <summary>
			/// The most recently issued request's response task
			/// </summary>
			task<http_response> pending_request_;

			/// <summary>
			/// The source of cancellation tokens for all requests
			/// </summary>
			cancellation_token_source request_async_src_;
		};
	}
}