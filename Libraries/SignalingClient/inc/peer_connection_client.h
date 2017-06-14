#pragma once

#include <mutex>
#include <string>
#include <functional>
#include <map>

#include <webrtc/base/thread.h>
#include <webrtc/base/messagehandler.h>
#include <webrtc/base/sigslot.h>

#include "OrderedHttpClient.h"

using namespace std;
using namespace concurrency;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace SignalingClient::Http;
using namespace rtc;

class PeerConnectionClient : public sigslot::has_slots<>,
    public rtc::MessageHandler
{
public:
    constexpr static int DefaultId = -1;

    PeerConnectionClient();
    ~PeerConnectionClient();

    void SignIn(string hostname, uint32_t port, string clientName);
    void SignOut();

    void set_proxy(const string& proxy);

    const map<int, string> peers();
    const int id();
    const string name();

    void SendToPeer(int clientId, string data);
    void SendHangUp(int clientId);

    // implements the MessageHandler interface
    void OnMessage(Message* msg);

    struct Observer
    {
        virtual void OnSignedIn() = 0;  // Called when we're logged on.

        virtual void OnDisconnected() = 0;

        virtual void OnPeerConnected(int id, const std::string& name) = 0;

        virtual void OnPeerDisconnected(int peer_id) = 0;

        virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;

        virtual void OnMessageSent(int err) = 0;

        virtual void OnServerConnectionFailure() = 0;

    protected:
        virtual ~Observer() {}
    };

    void RegisterObserver(Observer* observer);

private:
    void Poll();
    void UpdatePeers(string body);
    void RTCWrapAndCall(const function<void()>& func);

    OrderedHttpClient m_controlClient;
    OrderedHttpClient m_pollingClient;

    task<void> m_signInChain;
    task<void> m_signOutChain;
    task<void> m_sendToPeerChain;
    task<void> m_pollChain;

    map<int, string> m_peers;
    int m_id;
    string m_name;
    bool m_allowPolling;
    Observer* m_observer;
    mutex m_wrapAndCallMutex;
    rtc::Thread* m_signalingThread;

    struct BodyAndPragma
    {
        string body;
        string pragma;
    };
};