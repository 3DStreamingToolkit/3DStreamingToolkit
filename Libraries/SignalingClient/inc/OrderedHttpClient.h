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
            http_client request_client_;
            task<http_response> pending_request_;
            cancellation_token_source request_async_src_;
        };
    }
}