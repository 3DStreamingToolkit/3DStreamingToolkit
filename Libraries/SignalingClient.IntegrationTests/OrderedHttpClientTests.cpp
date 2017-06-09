#include "stdafx.h"
#include "CppUnitTest.h"

#include <chrono>
#include <cpprest/http_listener.h>

#include <OrderedHttpClient.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SignalingClient::Http;

namespace SignalingClientIntegrationTests
{		
	TEST_CLASS(OrderedHttpClientTests)
	{
	public:
        // TODO: find a free port at runtime
        // note: 3456 is used because it's more likely you have something else running on 3k on your devbox
        OrderedHttpClientTests() :
            listener_(uri(conversions::to_string_t("http://localhost:3456"))),
            listener_started_(false)
        {
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            if (listener_started_)
            {
                listener_.close().wait();
            }
        }

        /// <summary>
        /// Basic request test
        /// </summary>
		TEST_METHOD(BasicSingleRequest)
		{
            auto uri = SetAndStartHandler([](http_request req)
            {
                req.reply(status_codes::OK);
            });

            OrderedHttpClient client(uri);

            client.Issue(methods::GET, "/")
                .then([](http_response res)
            {
                Assert::IsTrue(res.status_code() == 200, L"expected a 200 status");
            }).wait();
		}

        /// <summary>
        /// Basic querystring test
        /// </summary>
        TEST_METHOD(QuerySingleRequest)
        {
            auto uri = SetAndStartHandler([](http_request req)
            {
                Assert::IsTrue(req.relative_uri().query().compare(L"api-version=1") == 0, L"expected querystring ?api-version=1");
                req.reply(status_codes::OK);
            });

            OrderedHttpClient client(uri);

            uri_builder builder;
            builder.append_path(L"sign_in");
            builder.append_query(L"api-version", L"1");

            client.Issue(methods::GET, builder.to_string())
                .then([](http_response res)
            {
                Assert::IsTrue(res.status_code() == 200, L"expected a 200 status");
            }).wait();
        }

        /// <summary>
        /// Ensures requests are sent in order (and recieved by the server in order)
        /// </summary>
        TEST_METHOD(BasicManyRequests)
        {
            std::wstring message;
            int handledCount = 0;
            auto uri = SetAndStartHandler([&](http_request req)
            {
                message += req.relative_uri().path();
                handledCount++;

                return req.reply(status_codes::OK);
            });

            OrderedHttpClient client(uri);

            vector<task<http_response>> tasks;

            tasks.push_back(client.Issue(methods::GET, "we"));
            tasks.push_back(client.Issue(methods::GET, "are"));
            tasks.push_back(client.Issue(methods::GET, "testing"));
            tasks.push_back(client.Issue(methods::GET, "all"));
            tasks.push_back(client.Issue(methods::GET, "the"));
            tasks.push_back(client.Issue(methods::GET, "things"));

            when_all(begin(tasks), end(tasks)).wait();

            Assert::AreEqual<wstring>(wstring(L"/we/are/testing/all/the/things"), message);
            Assert::AreEqual<int>(6, handledCount);
        }

        /// <summary>
        /// Ensures individual tasks returned by Issue() are valid and resolve in order
        /// </summary>
        TEST_METHOD(TimedManyRequests)
        {
            auto uri = SetAndStartHandler([&](http_request req)
            {
                return req.reply(status_codes::OK);
            });

            OrderedHttpClient client(uri);

            vector<chrono::time_point<chrono::steady_clock>> startTimes;
            vector<chrono::time_point<chrono::steady_clock>> endTimes;

            startTimes.push_back(chrono::steady_clock::now());
            client.Issue(methods::GET, "/")
                .then([&](http_response) { endTimes.push_back(chrono::steady_clock::now()); });
            startTimes.push_back(chrono::steady_clock::now());
            client.Issue(methods::GET, "/")
                .then([&](http_response) { return endTimes.push_back(chrono::steady_clock::now()); });
            startTimes.push_back(chrono::steady_clock::now());
            client.Issue(methods::GET, "/")
                .then([&](http_response) { endTimes.push_back(chrono::steady_clock::now()); });

            // since this should all be ordered, we can block on a new request
            // and rely on that block to indicate the above is complete
            client.Issue(methods::GET, "/").wait();

            auto prev = chrono::steady_clock::now();
            for each (auto st in startTimes)
            {
                Assert::IsTrue(chrono::duration<double, milli>(prev - st).count() > 0);
            }
            for each (auto st in endTimes)
            {
                Assert::IsTrue(chrono::duration<double, milli>(prev - st).count() > 0);
            }
        }

    private:
        string_t SetAndStartHandler(const function<void(http_request)>& func)
        {
            listener_.support(func);
            listener_.open().wait();
            listener_started_ = true;

            return listener_.uri().to_string();
        }

        bool listener_started_;
        experimental::listener::http_listener listener_;
	};
}