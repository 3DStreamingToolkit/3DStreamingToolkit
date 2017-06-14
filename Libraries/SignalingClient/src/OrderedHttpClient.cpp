#include "OrderedHttpClient.h"

namespace SignalingClient
{
    namespace Http
    {
        bool OrderedHttpClient::IsDefaultInitialized(const OrderedHttpClient& client)
        {
            return client.default_init_;
        }

        OrderedHttpClient::OrderedHttpClient() :
            OrderedHttpClient("http://*/")
        {
            default_init_ = true;
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
            request_root_(baseUrl),
            request_config_(config),
            default_init_(false)
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
            
            // handle (and silently swallow) code 105 failures
            // as these occur when we cancel a task (as we did just)
            // and is legal behavior in a destruction scenario
            try
            {
                pending_request_.wait();
            }
            catch (const exception& ex)
            {
                // swallow failures in the underlying task, as we're trying to kill it
            }
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
            auto captureRoot = request_root_;
            auto captureConfig = request_config_;

            if (pending_request_.is_done())
            {
                pending_request_ = create_task([] {
                    return http_response();
                });
            }

            return pending_request_ = pending_request_.then([=](http_response) {
                return http_client(captureRoot, captureConfig).request(requestMethod,
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
            return request_config_;
        }

        void OrderedHttpClient::SetConfig(const http_client_config& config)
        {
            request_config_ = config;
        }

        const uri OrderedHttpClient::uri()
        {
            return request_root_;
        }

        void OrderedHttpClient::SetUri(const web::uri& uri)
        {
            request_root_ = uri;
        }

        const task<http_response> OrderedHttpClient::most_recent()
        {
            return pending_request_;
        }
    }
}