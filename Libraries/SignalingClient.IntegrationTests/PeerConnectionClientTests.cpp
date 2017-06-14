#include "stdafx.h"
#include "CppUnitTest.h"

#include <chrono>
#include <cpprest/http_listener.h>

#define WEBRTC_WIN
#include <peer_connection_client.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dmoguids.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "msdmo.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ws2_32.lib") 
#pragma comment(lib, "common_video.lib")
#pragma comment(lib, "webrtc.lib")
#pragma comment(lib, "boringssl_asm.lib")
#pragma comment(lib, "field_trial_default.lib")
#pragma comment(lib, "metrics_default.lib")
#pragma comment(lib, "protobuf_full.lib")

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SignalingClient;

namespace SignalingClientIntegrationTests
{
    TEST_CLASS(PeerConnectionClientTests)
    {
    public:
        // TODO: find a free port at runtime
        // note: 3456 is used because it's more likely you have something else running on 3k on your devbox
        PeerConnectionClientTests() :
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
        /// Basic sign in test
        /// </summary>
        TEST_METHOD(PeerLogin)
        {
            auto uri = SetAndStartHandler([](http_request req)
            {
                http_response res;
                if (req.relative_uri().path().compare(L"/sign_in") == 0)
                {
                    res.set_status_code(200);
                    res.headers().add(L"Pragma", 3);
                    res.set_body("renderingtest_user3@machine,3,1\nrenderingtest_user2@machine,2,1\nrenderingtest_user@machine,1,1");
                }

                return req.reply(res);
            });

            PeerConnectionClient client;
            client.RegisterObserver(&observer);

            client.SignIn(utility::conversions::to_utf8string(uri.host()), uri.port(), "renderingtest_user3@machine");

            // if this runs indefintely, it's a failure
            observer.onSignedIn.wait();

            Assert::AreEqual<int>(3, client.id());

            auto peers = client.peers();
            Assert::AreEqual<size_t>(2, peers.size());
            Assert::AreEqual<string>("renderingtest_user2@machine", peers[2]);
            Assert::AreEqual<string>("renderingtest_user@machine", peers[1]);
        }

        /// <summary>
        /// Basic sign out test
        /// </summary>
        TEST_METHOD(PeerLogout)
        {
            auto uri = SetAndStartHandler([](http_request req)
            {
                http_response res;
                if (req.relative_uri().path().compare(L"/sign_in") == 0)
                {
                    res.set_status_code(200);
                    res.headers().add(L"Pragma", 3);
                    res.set_body("renderingtest_user3@machine,3,1\nrenderingtest_user2@machine,2,1\nrenderingtest_user@machine,1,1");
                }
                else if (req.relative_uri().path().compare(L"/sign_out") == 0)
                {
                    res.set_status_code(200);
                }

                return req.reply(res);
            });

            PeerConnectionClient client;
            client.RegisterObserver(&observer);

            client.SignIn(utility::conversions::to_utf8string(uri.host()), uri.port(), "renderingtest_user3@machine");

            // if this runs indefintely, it's a failure
            observer.onSignedIn.wait();

            Assert::AreNotEqual<int>(-1, client.id());
            Assert::AreNotEqual<size_t>(0, client.peers().size());

            client.SignOut();

            // if this runs indefintely, it's a failure
            observer.onDisconnected.wait();

            Assert::AreEqual<int>(-1, client.id());
            Assert::AreEqual<size_t>(0, client.peers().size());
        }

        /// <summary>
        /// Basic full session
        /// </summary>
        TEST_METHOD(PeerSession)
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
                else if (method == methods::POST && path.compare(L"/message") == 0 && query.compare(L"peer_id=1&to=2") == 0)
                {
                    response.set_status_code(200);
                }
                else if (method == methods::GET && path.compare(L"/wait") == 0 && query.compare(L"peer_id=1") == 0)
                {
                    response.set_body(waitingBody);
                    response.set_status_code(200);
                    response.headers().set_content_type(L"application/json");
                    response.headers().add(L"Pragma", L"2");
                }
                else if (method == methods::GET && path.compare(L"/sign_out") == 0 && query.compare(L"peer_id=1") == 0)
                {
                    response.set_status_code(200);
                }

                return req.reply(response);
            });

            PeerConnectionClient client;
            client.RegisterObserver(&observer);

            // connect and login
            client.SignIn(utility::conversions::to_utf8string(uri.host()), uri.port(), "renderingtest_user@machine");

            observer.onSignedIn.wait();

            Assert::AreEqual<int>(1, client.id());
            Assert::AreEqual<size_t>(1, client.peers().size());

            client.SendToPeer(2, "hi mom");

            auto statusCode = observer.onMessageSent.get();

            Assert::AreEqual<int>(200, statusCode);

            auto msg = observer.onMessageFromPeer.get();

            Assert::AreEqual<int>(2, msg.i);
            Assert::AreEqual<string>(waitingBody, msg.s);

            client.SignOut();

            observer.onDisconnected.wait();

            Assert::AreEqual<size_t>(0, client.peers().size());
        }

    private:
        uri SetAndStartHandler(const function<void(http_request)>& func)
        {
            listener_.support(func);
            listener_.open().wait();
            listener_started_ = true;

            return listener_.uri();
        }

        bool listener_started_;
        experimental::listener::http_listener listener_;

        struct Observer : public PeerConnectionClient::Observer
        {
        public:

            struct IntStrBox
            {
                int i;
                string s;
            };

            Observer()
            {
                onSignedIn = task<void>(onSignedInTrigger);
                onDisconnected = task<void>(onDisconnectedTrigger);
                onPeerConnected = task<IntStrBox>(onPeerConnectedTrigger);
                onPeerDisconnected = task<int>(onPeerDisconnectedTrigger);
                onMessageFromPeer = task<IntStrBox>(onMessageFromPeerTrigger);
                onMessageSent = task<int>(onMessageSentTrigger);
                onServerConnectionFailure = task<void>(onServerConnectionFailureTrigger);
            }
            
            void OnSignedIn()
            {
                onSignedInTrigger.set();
            }

            void OnDisconnected()
            {
                onDisconnectedTrigger.set();
            }

            void OnPeerConnected(int id, const std::string& name)
            {
                IntStrBox b;
                b.i = id;
                b.s = name;

                onPeerConnectedTrigger.set(b);
            }

            void OnPeerDisconnected(int id)
            {
                onPeerDisconnectedTrigger.set(id);
            }

            void OnMessageFromPeer(int peer_id, const std::string& message)
            {
                IntStrBox b;
                b.i = peer_id;
                b.s = message;

                onMessageFromPeerTrigger.set(b);
            }

            void OnMessageSent(int err)
            {
                onMessageSentTrigger.set(err);
            }

            void OnServerConnectionFailure()
            {
                onServerConnectionFailureTrigger.set();
            }

            task<void> onSignedIn;
            task_completion_event<void> onSignedInTrigger;
            task<void> onDisconnected;
            task_completion_event<void> onDisconnectedTrigger;
            task<IntStrBox> onPeerConnected;
            task_completion_event<IntStrBox> onPeerConnectedTrigger;
            task<int> onPeerDisconnected;
            task_completion_event<int> onPeerDisconnectedTrigger;
            task<IntStrBox> onMessageFromPeer;
            task_completion_event<IntStrBox> onMessageFromPeerTrigger;
            task<int> onMessageSent;
            task_completion_event<int> onMessageSentTrigger;
            task<void> onServerConnectionFailure;
            task_completion_event<void> onServerConnectionFailureTrigger;
        };

        Observer observer;
    };
}