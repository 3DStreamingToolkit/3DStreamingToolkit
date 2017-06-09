#include "OrderedHttpClient.h"

namespace SignalingClient
{
    namespace Http
    {
        OrderedHttpClient::OrderedHttpClient() :
            OrderedHttpClient("http://*/")
        {
        }

        OrderedHttpClient::OrderedHttpClient(const string& baseUrl) :
            OrderedHttpClient(conversions::to_string_t(baseUrl))
        {
        }

        OrderedHttpClient::OrderedHttpClient(const string_t& baseUrl) :
            OrderedHttpClient(baseUrl, http_client_config())
        {
        }

        OrderedHttpClient::OrderedHttpClient(const string_t& baseUrl, const http_client_config& config) :
            request_client_(baseUrl, config)
        {
            // the contents of the first task don't matter, as
            // it's instantly resolved, and therefore overwritten
            // on the first call to Issue()
            pending_request_ = create_task([] {
                return http_response();
            });
        }

        OrderedHttpClient::~OrderedHttpClient()
        {
            CancelAll();
        }

        task<http_response> OrderedHttpClient::Issue(const method& requestMethod, const string& requestEndpoint)
        {
            return Issue(requestMethod, conversions::to_string_t(requestEndpoint));
        }

        task<http_response> OrderedHttpClient::Issue(const method& requestMethod, const string& requestEndpoint, const string& requestBody)
        {
            return Issue(requestMethod, conversions::to_string_t(requestEndpoint), conversions::to_string_t(requestBody));
        }

        task<http_response> OrderedHttpClient::Issue(const method& requestMethod, const string_t& requestEndpoint)
        {
            return Issue(requestMethod, requestEndpoint, conversions::to_string_t(""));
        }

        task<http_response> OrderedHttpClient::Issue(const method& requestMethod, const string_t& requestEndpoint, const string_t& requestBody)
        {
            return pending_request_ = pending_request_.then([=](http_response) {
                return request_client_.request(requestMethod,
                    requestEndpoint,
                    requestBody,
                    request_async_src_.get_token());
            });
        }

        void OrderedHttpClient::CancelAll()
        {
            request_async_src_.cancel();
            request_async_src_ = cancellation_token_source();
        }

        const http_client_config OrderedHttpClient::config()
        {
            return request_client_.client_config();
        }

        void OrderedHttpClient::SetConfig(const http_client_config& config)
        {
            request_client_ = http_client(request_client_.base_uri(), config);
        }

        const uri OrderedHttpClient::uri()
        {
            return request_client_.base_uri();
        }

        void OrderedHttpClient::SetUri(const web::uri& uri)
        {
            request_client_ = http_client(uri);
        }

        const task<http_response> OrderedHttpClient::most_recent()
        {
            return pending_request_;
        }
    }
}