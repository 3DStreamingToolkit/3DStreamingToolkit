/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "peer_connection_client.h"
#include "webrtc/base/logging.h"

namespace
{
    // This is our magical hangup signal.
    const char kByeMessage[] = "BYE";

    // Delay between server connection retries, in milliseconds
    const int kReconnectDelay = 2000;
}

PeerConnectionClient::PeerConnectionClient() :
    callback_(NULL),
    state_(NOT_CONNECTED),
    my_id_(-1)
{
}

PeerConnectionClient::~PeerConnectionClient()
{
}

int PeerConnectionClient::id() const
{
    return my_id_;
}

bool PeerConnectionClient::is_connected() const
{
    return my_id_ != -1;
}

const Peers& PeerConnectionClient::peers() const
{
    return peers_;
}

void PeerConnectionClient::RegisterObserver(PeerConnectionClientObserver* callback)
{
    RTC_DCHECK(!callback_);
    callback_ = callback;
}

void PeerConnectionClient::Connect(const std::string& server, int port,
    const std::string& client_name)
{
    RTC_DCHECK(!server.empty());
    RTC_DCHECK(!client_name.empty());

    if (state_ != NOT_CONNECTED)
    {
        LOG(WARNING) << "The client must not be connected before you can call Connect()";
        callback_->OnServerConnectionFailure();
        return;
    }

    if (server.empty() || port <= 0 || client_name.empty())
    {
        callback_->OnServerConnectionFailure();
        return;
    }

    std::string hostname;
    web::uri_builder uri;

    if (server.substr(0, 7).compare("http://") == 0)
    {
        uri.set_scheme(L"http");
        hostname = server.substr(7);
    }
    else if (server.substr(0, 8).compare("https://") == 0)
    {
        uri.set_scheme(L"https");
        hostname = server.substr(8);
    }
    else
    {
        hostname = server;
    }

    uri.set_host(utility::conversions::to_string_t(hostname));
    uri.set_port(utility::conversions::to_string_t(std::to_string(port)));

    server_address_ = uri.to_uri();
    client_name_ = utility::conversions::to_string_t(client_name);

    auto config = CreateHttpConfig();

    // cancel any pending requests
    control_request_client_.CancelAll();
    polling_request_client_.CancelAll();

    control_request_client_.SetUri(server_address_);
    control_request_client_.SetConfig(config);

    polling_request_client_.SetUri(server_address_);
    polling_request_client_.SetConfig(config);

    DoConnect();
}

void PeerConnectionClient::DoConnect()
{
    if (state_ == CONNECTED || state_ == SIGNING_IN)
    {
        return;
    }

    web::uri_builder builder;
    builder.append_path(L"sign_in");
    builder.append_query(client_name_);

    state_ = SIGNING_IN;

    // schedule the async work to make the request and process it's body (calling the callbacks)
    control_request_client_.Issue(web::http::methods::GET, builder.to_string())
        .then([&](web::http::http_response res) { state_ = NOT_CONNECTED; return res; })
        .then(SignalingClient::ExpectStatus(200, false))
        .then(SignalingClient::LogError("signing in failed"))
        .then([&](web::http::http_response res)
    {
        auto pragmaHeader = res.headers()[L"Pragma"];
        my_id_ = static_cast<int>(wcstol(pragmaHeader.c_str(), nullptr, 10));

        // return the async operation of parsing and processing the body
        return SignalingClient::ParseBodyA()(res)
            .then(SignalingClient::SplitString('\n'))
            .then([&](std::vector<std::string> lines)
        {
            // parse each line as a peer and fire the callback
            for each (auto line in lines)
            {
                int id = 0;
                std::string name;
                bool connected;

                if (ParseEntry(line, &name, &id, &connected) && id != my_id_)
                {
                    peers_[id] = name;
                    callback_->OnPeerConnected(id, name);
                }
            }
        })
            .then([&]()
        {
            // we're signed in
            state_ = CONNECTED;
            callback_->OnSignedIn();

            ConfigurePolling();
        });
    });
}

void PeerConnectionClient::ConfigurePolling()
{
    if (state_ != CONNECTED)
    {
        return;
    }

    web::uri_builder builder;
    builder.append_path(L"wait");
    builder.append_query(L"peer_id", std::to_wstring(my_id_));

    polling_request_client_.Issue(web::http::methods::GET, builder.to_string())
        .then([&](web::http::http_response res) { ConfigurePolling(); return res; })
        .then(SignalingClient::ExpectStatus(200))
        .then(SignalingClient::LogError("waiting failed"))
        .then([&](web::http::http_response res)
    {
        auto pragmaHeader = res.headers()[L"Pragma"];
        auto peer_id = static_cast<int>(wcstol(pragmaHeader.c_str(), nullptr, 10));

        // parse peers list, or pass message upward
        return SignalingClient::ParseBodyA()(res)
            .then([&, peer_id](std::string body)
        {
            if (my_id_ != peer_id)
            {
                OnMessageFromPeer(peer_id, body);
                concurrency::cancel_current_task();
            }

            return body;
        })
            .then(SignalingClient::SplitString('\n'))
            .then([&](std::vector<std::string> lines)
        {
            for each (auto line in lines)
            {
                int id = 0;
                std::string name;
                bool connected;

                if (ParseEntry(line, &name, &id, &connected))
                {
                    if (connected)
                    {
                        peers_[id] = name;
                        callback_->OnPeerConnected(id, name);
                    }
                    else
                    {
                        peers_.erase(id);
                        callback_->OnPeerDisconnected(id);
                    }
                }
            }
        });
    });
}

bool PeerConnectionClient::SendToPeer(int peer_id, const std::string& message)
{
    if (state_ != CONNECTED)
    {
        return false;
    }

    RTC_DCHECK(is_connected());
    if (!is_connected() || peer_id == -1)
    {
        return false;
    }

    web::uri_builder builder;
    builder.append_path(L"message");

    // the naming here isn't a bug, it's just confusing
    builder.append_query(L"peer_id", std::to_wstring(my_id_));
    builder.append_query(L"to", std::to_wstring(peer_id));

    // make the request
    control_request_client_.Issue(web::http::methods::POST, builder.to_string(), utility::conversions::to_string_t(message))
        .then(SignalingClient::LogError("sending message failed"))
        .then([&](web::http::http_response res)
    {
        callback_->OnMessageSent(res.status_code());
    });

    return true;
}

bool PeerConnectionClient::SendHangUp(int peer_id)
{
    return SendToPeer(peer_id, kByeMessage);
}

bool PeerConnectionClient::IsSendingMessage()
{
    // since our http implementation internally buffers messages
    // we can always "send" more, so we mark ourselves as never stuck sending messages
    return false;
}

bool PeerConnectionClient::SignOut()
{
    if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
    {
        return true;
    }

    state_ = SIGNING_OUT;

    if (my_id_ != -1)
    {
        web::uri_builder builder;
        builder.append_path(L"sign_out");
        builder.append_query(L"peer_id", std::to_wstring(my_id_));

        state_ = SIGNING_OUT_WAITING;

        // make the request
        control_request_client_.Issue(web::http::methods::GET, builder.to_string())
            .then(SignalingClient::ExpectStatus(200))
            .then(SignalingClient::LogError("signing out failed"))
            .then([&](web::http::http_response)
        {
            Close();
            callback_->OnDisconnected();
        });

        return true;
    }
    else
    {
        // Can occur if the app is closed before we finish connecting.
        return true;
    }

    return true;
}

void PeerConnectionClient::Close()
{
    peers_.clear();

    my_id_ = -1;
    state_ = NOT_CONNECTED;
}

void PeerConnectionClient::OnMessageFromPeer(int peer_id, const std::string& message)
{
    if (message.length() == (sizeof(kByeMessage) - 1) &&
        message.compare(kByeMessage) == 0)
    {
        callback_->OnPeerDisconnected(peer_id);
    }
    else
    {
        callback_->OnMessageFromPeer(peer_id, message);
    }
}

web::http::client::http_client_config PeerConnectionClient::CreateHttpConfig()
{
    web::http::client::http_client_config config;
    config.set_timeout(utility::seconds(60 * 5));
    config.set_validate_certificates(true);

    if (proxy_address_.length() > 0)
    {
        config.set_proxy(web::web_proxy(utility::conversions::to_string_t(proxy_address_)));
    }

    return config;
}

bool PeerConnectionClient::ParseEntry(const std::string& entry, std::string* name,
    int* id, bool* connected)
{
    RTC_DCHECK(name != NULL);
    RTC_DCHECK(id != NULL);
    RTC_DCHECK(connected != NULL);
    RTC_DCHECK(!entry.empty());

    *connected = false;
    size_t separator = entry.find(',');
    if (separator != std::string::npos)
    {
        // assumption here is that the id of an entry is representable as an int
        *id = atoi(&entry[separator + 1]);

        name->assign(utility::conversions::to_utf8string(entry.substr(0, separator)));
        separator = entry.find(',', separator + 1);

        if (separator != std::string::npos)
        {
            *connected = atoi(&entry[separator + 1]) ? true : false;
        }
    }

    return !name->empty();
}

void PeerConnectionClient::OnMessage(rtc::Message* msg)
{
    // ignore msg; there is currently only one supported message ("retry")
    DoConnect();
}

void PeerConnectionClient::set_proxy(const std::string& proxy)
{
    proxy_address_ = proxy;
}

const std::string& PeerConnectionClient::proxy() const
{
    return proxy_address_;
}