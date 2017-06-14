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

        TEST_METHOD(FullSession)
        {
            auto signInBody = "renderingtest_user@machine,1,1\nrenderingtest_user2@machine,2,1";
            auto waitingBody = "{\"sdp\" : \"v=0\r\no=- 5201091431656185443 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE video data\r\na=msid-semantic: WMS stream_label\r\nm=video 9 UDP/TLS/RTP/SAVPF 100 96 98 127 125 97 99 101 124\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:tLaS\r\na=ice-pwd:tfVDYUeI3cQ77Mrzr/6S+Aab\r\na=fingerprint:sha-256 66:2B:38:3E:1B:F6:97:A5:3C:76:B7:77:9B:D3:41:9E:B8:17:49:7F:AC:E0:2A:20:4F:7F:9B:1E:F9:0E:5F:F3\r\na=setup:actpass\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\na=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\na=sendrecv\r\na=rtcp-mux\r\na=rtcp-rsize\r\na=rtpmap:100 H264/90000\r\na=rtcp-fb:100 ccm fir\r\na=rtcp-fb:100 nack\r\na=rtcp-fb:100 nack pli\r\na=rtcp-fb:100 goog-remb\r\na=rtcp-fb:100 transport-cc\r\na=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\na=rtpmap:96 VP8/90000\r\na=rtcp-fb:96 ccm fir\r\na=rtcp-fb:96 nack\r\na=rtcp-fb:96 nack pli\r\na=rtcp-fb:96 goog-remb\r\na=rtcp-fb:96 transport-cc\r\na=rtpmap:98 VP9/90000\r\na=rtcp-fb:98 ccm fir\r\na=rtcp-fb:98 nack\r\na=rtcp-fb:98 nack pli\r\na=rtcp-fb:98 goog-remb\r\na=rtcp-fb:98 transport-cc\r\na=rtpmap:127 red/90000\r\na=rtpmap:125 ulpfec/90000\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=96\r\na=rtpmap:99 rtx/90000\r\na=fmtp:99 apt=98\r\na=rtpmap:101 rtx/90000\r\na=fmtp:101 apt=100\r\na=rtpmap:124 rtx/90000\r\na=fmtp:124 apt=127\r\na=ssrc-group:FID 774013143 3250020170\r\na=ssrc:774013143 cname:Nh+hnIcAof2YcXCH\r\na=ssrc:774013143 msid:stream_label video_label\r\na=ssrc:774013143 mslabel:stream_label\r\na=ssrc:774013143 label:video_label\r\na=ssrc:3250020170 cname:Nh+hnIcAof2YcXCH\r\na=ssrc:3250020170 msid:stream_label video_label\r\na=ssrc:3250020170 mslabel:stream_label\r\na=ssrc:3250020170 label:video_label\r\nm=application 9 DTLS/SCTP 5000\r\nc=IN IP4 0.0.0.0\r\na=ice-ufrag:tLaS\r\na=ice-pwd:tfVDYUeI3cQ77Mrzr/6S+Aab\r\na=fingerprint:sha-256 66:2B:38:3E:1B:F6:97:A5:3C:76:B7:77:9B:D3:41:9E:B8:17:49:7F:AC:E0:2A:20:4F:7F:9B:1E:F9:0E:5F:F3\r\na=setup:actpass\r\na=mid:data\r\na=sctpmap:5000 webrtc-datachannel 1024\r\n\",\"type\" : \"offer\"}";

            // mock the protocol
            auto uri = SetAndStartHandler([&](http_request req)
            {
                auto method = req.method();
                auto path = req.relative_uri().path();
                auto query = req.relative_uri().query();

                http_response response(status_codes::ServiceUnavailable);

                if (method == methods::GET && path.compare(L"/sign_in") == 0 && query.compare(L"renderingtest_user@machine") == 0)
                {
                    response.set_body(signInBody);
                    response.headers().add(L"Pragma", L"1");
                    response.set_status_code(200);
                }
                else if (method == methods::POST && path.compare(L"/message") == 0 && query.compare(L"peer_id=2&to=1") == 0)
                {
                    response.set_status_code(200);
                }
                else if (method == methods::GET && path.compare(L"/wait") == 0 && query.compare(L"peer_id=1") == 0)
                {
                    response.set_body(waitingBody);
                    response.set_status_code(200);
                    response.headers().set_content_type(L"application/json");
                }
                else if (method == methods::GET && path.compare(L"/sign_out") == 0 && query.compare(L"peer_id=1") == 0)
                {
                    response.set_status_code(200);
                }

                return req.reply(response);
            });

            OrderedHttpClient client(uri);

            // test sign in
            uri_builder builder;
            builder.append_path(L"sign_in");
            builder.append_query(L"renderingtest_user@machine");

            auto signedIn = client.Issue(methods::GET, builder.to_string()).get();

            // validate
            Assert::AreEqual<int>(200, signedIn.status_code());
            Assert::AreEqual<string>(signInBody, signedIn.extract_utf8string(true).get());

            // test messaging
            builder.clear();
            builder.append_path(L"message");

            // the naming here isn't a bug, it's just confusing (1 is us)
            builder.append_query(L"peer_id", std::to_wstring(2));
            builder.append_query(L"to", std::to_wstring(1));

            auto messageOneFromTwo = client.Issue(methods::POST, conversions::to_utf8string(builder.to_string()), waitingBody).get();

            // validate
            Assert::AreEqual<int>(200, messageOneFromTwo.status_code());

            // test waiting
            builder.clear();
            builder.append_path(L"wait");
            builder.append_query(L"peer_id", std::to_wstring(1));

            auto getMessage = client.Issue(methods::GET, builder.to_string()).get();

            // validate
            Assert::AreEqual<int>(200, getMessage.status_code());
            Assert::AreEqual<string>(waitingBody, getMessage.extract_utf8string(true).get());

            // test sign out
            builder.clear();
            builder.append_path(L"sign_out");
            builder.append_query(L"peer_id", std::to_wstring(1));

            auto signedOut = client.Issue(methods::GET, builder.to_string()).get();

            // validate
            Assert::AreEqual<int>(200, signedOut.status_code());
        }

        TEST_METHOD(HangingGetOnDtor)
        {
            // mock the protocol
            auto uri = SetAndStartHandler([&](http_request req)
            {
                // no response.
            });
            
            bool requestCompleted = false;
            auto client = new OrderedHttpClient(uri);
            auto tsk = client->Issue(methods::GET, "/")
                .then([&](http_response)
            {
                requestCompleted = true;
            });

            delete client;
            
            try
            {
                tsk.wait();
            }
            catch (const http_exception& ex)
            {
                Assert::AreEqual<int>(105, ex.error_code().value());
            }

            Assert::AreEqual<bool>(false, requestCompleted);
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