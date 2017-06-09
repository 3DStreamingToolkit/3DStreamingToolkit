/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PEER_CONNECTION_CLIENT_H_
#define WEBRTC_PEER_CONNECTION_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <iostream>
#include <functional>

#include <cpprest/http_client.h>

#include "webrtc/base/signalthread.h"
#include "webrtc/base/sigslot.h"

#include "OrderedHttpClient.h"
#include "peer_connection_async.h"

typedef std::map<int, std::string> Peers;

struct PeerConnectionClientObserver
{
    virtual void OnSignedIn() = 0;  // Called when we're logged on.

    virtual void OnDisconnected() = 0;

    virtual void OnPeerConnected(int id, const std::string& name) = 0;

    virtual void OnPeerDisconnected(int peer_id) = 0;

    virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;

    virtual void OnMessageSent(int err) = 0;

    virtual void OnServerConnectionFailure() = 0;

protected:
    virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient : public sigslot::has_slots<>,
    public rtc::MessageHandler
{
public:
    enum State
    {
        NOT_CONNECTED,
        SIGNING_IN,
        CONNECTED,
        SIGNING_OUT_WAITING,
        SIGNING_OUT,
    };

    PeerConnectionClient();

    ~PeerConnectionClient();

    int id() const;

    bool is_connected() const;

    const Peers& peers() const;

    void RegisterObserver(PeerConnectionClientObserver* callback);

    void Connect(const std::string& server, int port,
        const std::string& client_name);

    bool SendToPeer(int peer_id, const std::string& message);

    bool SendHangUp(int peer_id);

    bool IsSendingMessage();

    bool SignOut();

    // implements the MessageHandler interface
    void OnMessage(rtc::Message* msg);

    void set_proxy(const std::string& proxy);

    const std::string& proxy() const;

protected:
    void DoConnect();

    void ConfigurePolling();

    void Close();

    void OnMessageFromPeer(int peer_id, const std::string& message);

    // Parses a single line entry in the form "<name>,<id>,<connected>"
    bool ParseEntry(const std::string& entry, std::string* name, int* id,
        bool* connected);

    SignalingClient::Http::http_client_config CreateHttpConfig();
    SignalingClient::Http::uri server_address_;
    std::string proxy_address_;
    std::wstring client_name_;
    SignalingClient::Http::OrderedHttpClient control_request_client_;
    SignalingClient::Http::OrderedHttpClient polling_request_client_;

    PeerConnectionClientObserver* callback_;
    Peers peers_;
    State state_;
    int my_id_;
};
#endif  // WEBRTC_PEER_CONNECTION_CLIENT_H_
