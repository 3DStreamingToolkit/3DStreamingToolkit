#pragma once

#include <string>
#include <cpprest/http_client.h>

namespace SignalingClient
{
    namespace Http
    {
        using namespace std;
        using namespace utility;
        using namespace concurrency;
        using namespace web::http;
        using namespace web::http::client;

        class OrderedHttpClient
        {
        public:
            static bool IsDefaultInitialized(const OrderedHttpClient& client);

            OrderedHttpClient();
            OrderedHttpClient(const string& baseUrl);
            OrderedHttpClient(const string_t& baseUrl);
            OrderedHttpClient(const string_t& baseUrl, const http_client_config& config);

            ~OrderedHttpClient();

            task<http_response> Issue(const method& requestMethod, const string& requestEndpoint);
            task<http_response> Issue(const method& requestMethod, const string& requestEndpoint, const string& requestBody);

            task<http_response> Issue(const method& requestMethod, const string_t& requestEndpoint);
            task<http_response> Issue(const method& requestMethod, const string_t& requestEndpoint, const string_t& requestBody);

            void CancelAll();

            const http_client_config config();
            void SetConfig(const http_client_config& config);

            const uri uri();
            void SetUri(const web::uri& uri);

            const task<http_response> most_recent();

        private:
            bool default_init_;
            web::uri request_root_;
            http_client_config request_config_;
            task<http_response> pending_request_;
            cancellation_token_source request_async_src_;
        };
    }
}