#pragma once

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
    PeerConnectionClient();
    ~PeerConnectionClient();

    void Connect(string hostname, uint32_t port);
    void Login(string clientName);
    void Logout();
    void Disconnect();

    void set_proxy(const string& proxy);

    const map<int, string> peers();
    const int id();

    void PushMessage(int clientId, string data);
    string PopMessage();

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

    map<int, string> m_peers;
    int m_id;
    queue<string> m_messageBuffer;
    bool m_allowPolling;
    Observer* m_observer;

    struct BodyAndPragma
    {
        string body;
        string pragma;
    };
};