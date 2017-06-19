#include "peer_connection_client.h"

PeerConnectionClient::PeerConnectionClient() :
    m_allowPolling(true)
{
    // we need a "signaling_thread" so that all our webrtc calls occur
    // on the same rtc::Thread
    auto wrapped = rtc::Thread::Current();

    if (wrapped == nullptr)
    {
        wrapped = rtc::ThreadManager::Instance()->WrapCurrentThread();
    }

    m_signalingThread = wrapped;

    // initialize our chains
    m_signInChain = create_task([] {});
    m_signOutChain = create_task([] {});
    m_sendToPeerChain = create_task([] {});
    m_pollChain = create_task([] {});
}

PeerConnectionClient::~PeerConnectionClient()
{
    m_allowPolling = false;

    // these occur when we cancel a task (as we did just)
    // and is legal behavior in a destruction scenario
    try
    {
        m_signInChain.wait();
        m_signOutChain.wait();
        m_sendToPeerChain.wait();
        m_pollChain.wait();
    }
    catch (const exception&)
    {
        // swallow failures in the underlying task, as we're trying to kill it
    }

    // wonder if this will fix mutex crash on shutdown
    lock_guard<recursive_mutex> guard(m_wrapAndCallMutex);
}

void PeerConnectionClient::SignIn(string hostname, uint32_t port, string clientName)
{
    if (id_ != DefaultId)
    {
        return;
    }

    uri_builder baseUriBuilder;

	if (hostname.substr(0, 7).compare("http://") == 0)
	{
		baseUriBuilder.set_scheme(L"http");
		hostname = hostname.substr(7);
	}
	else if (hostname.substr(0, 8).compare("https://") == 0)
	{
		baseUriBuilder.set_scheme(L"https");
		hostname = hostname.substr(8);
	}

    baseUriBuilder.set_host(conversions::to_utf16string(hostname));
    baseUriBuilder.set_port(port);

    m_controlClient.SetUri(baseUriBuilder.to_uri());
    m_pollingClient.SetUri(baseUriBuilder.to_uri());

    auto existingPollingConfig = m_pollingClient.config();
    existingPollingConfig.set_timeout(utility::seconds::max());

    name_ = clientName;

    m_signInChain = m_signInChain.then([&, clientName]
    {
        uri_builder endpointUriBuilder;
        endpointUriBuilder.append_path(L"/sign_in");
        endpointUriBuilder.append_query(utility::conversions::to_string_t(clientName));

        m_controlClient.Issue(methods::GET, endpointUriBuilder.to_string())
            .then([&](http_response res)
        {
            auto pragmaHeader = utility::conversions::to_utf8string(res.headers()[L"Pragma"]);

            return res.extract_utf8string(true)
                .then([&, pragmaHeader](string body)
            {
                BodyAndPragma box;
                box.body = body;
                box.pragma = pragmaHeader;

                return box;
            });
        })
            .then([&](BodyAndPragma box)
        {
            id_ = atoi(box.pragma.c_str());

            UpdatePeers(box.body);
        })
            .then([&]
        {
            // notify our observer
            RTCWrapAndCall([&] {
                if (observer_ != nullptr)
                {
                    observer_->OnSignedIn();
                }
            });

            // issue a polling request (will self-sustain)
            Poll();
        });
    });
}

void PeerConnectionClient::SignOut()
{
    if (id_ == DefaultId)
    {
        return;
    }

    m_signOutChain = m_signOutChain.then([&]
    {
        uri_builder builder;
        builder.append_path(L"sign_out");
        builder.append_query(L"peer_id", std::to_wstring(id_));

        m_controlClient.Issue(methods::GET, builder.to_string())
            .then([&](http_response) { id_ = -1; })
            .then([&]
        {
            m_allowPolling = false;
            m_controlClient.CancelAll();
            m_pollingClient.CancelAll();

            peers_.clear();

            // notify our observer
            RTCWrapAndCall([&] {
                if (observer_ != nullptr)
                {
                    observer_->OnDisconnected();
                }
            });
        });
    });
}

void PeerConnectionClient::SetProxy(const string& proxy)
{
    if (proxy.length() <= 0)
    {
        return;
    }

    {
        auto existing = m_controlClient.config();
        existing.set_proxy(web_proxy(uri(utility::conversions::to_string_t(proxy))));
        m_controlClient.SetConfig(existing);
    }

    {
        auto existing = m_pollingClient.config();
        existing.set_proxy(web_proxy(uri(utility::conversions::to_string_t(proxy))));
        m_pollingClient.SetConfig(existing);
    }
}

void PeerConnectionClient::SendToPeer(int clientId, string data)
{
    if (id_ == DefaultId)
    {
        return;
    }

    m_sendToPeerChain = m_sendToPeerChain.then([&, clientId, data]
    {
        uri_builder builder;
        builder.append_path(L"message");
        builder.append_query(L"peer_id", std::to_wstring(id_));
        builder.append_query(L"to", std::to_wstring(clientId));

        m_controlClient.Issue(methods::POST, builder.to_string(), utility::conversions::to_string_t(data))
            .then([&](http_response res)
        {
            auto status = res.status_code();

            // notify our observer
            RTCWrapAndCall([&, status] {
                if (observer_ != nullptr)
                {
                    observer_->OnMessageSent(status);
                }
            });
        });
    });
}

void PeerConnectionClient::SendHangUp(int clientId)
{
    SendToPeer(clientId, "BYE");
}

// ignore msg; there is currently only one supported message ("retry")
void PeerConnectionClient::OnMessage(rtc::Message*)
{
    if (OrderedHttpClient::IsDefaultInitialized(m_controlClient) ||
        OrderedHttpClient::IsDefaultInitialized(m_pollingClient) ||
        name_.length() <= 0)
    {
        // ERROR, default initialized means we can't "retry"
        return;
    }

    // use our existing uri values to reconnect
    auto uri = m_controlClient.uri();
    auto hostAsStr = utility::conversions::to_utf8string(uri.host());

    // try connecting
    SignIn(hostAsStr, uri.port(), name_);
}

rtc::Thread* PeerConnectionClient::signaling_thread()
{
	return m_signalingThread;
}

void PeerConnectionClient::Poll()
{
    if (!m_allowPolling)
    {
        return;
    }

    m_pollChain = m_pollChain.then([&]
    {
        uri_builder builder;
        builder.append_path(L"wait");
        builder.append_query(L"peer_id", std::to_wstring(id_));

        m_pollingClient.Issue(methods::GET, builder.to_string())
            .then([&](http_response res)
        {
            auto pragmaHeader = utility::conversions::to_utf8string(res.headers()[L"Pragma"]);

            return res.extract_utf8string(true)
                .then([&, pragmaHeader](string body)
            {
                BodyAndPragma box;
                box.body = body;
                box.pragma = pragmaHeader;

                return box;
            });
        })
            .then([&](BodyAndPragma box)
        {
            auto senderId = atoi(box.pragma.c_str());
            auto body = box.body;

            if (senderId == id_)
            {
                // note: we notify the observer of peer changes INSIDE UpdatePeers()
                UpdatePeers(body);
            }
            else
            {
                // notify our observer
                RTCWrapAndCall([&, senderId, body] {
                    if (observer_ != nullptr)
                    {
                        observer_->OnMessageFromPeer(senderId, body);
                    }
                });
            }
        })
            .then([&](task<void> prevTask)
        {
            try
            {
                prevTask.wait();
            }
            catch (exception&)
            {
                // swallow
            }

            Poll();
        });
    });
}

void PeerConnectionClient::UpdatePeers(string body)
{
    // split body into lines 
    vector<string> lines;
    auto linesInserter = back_inserter(lines);

    stringstream ss;
    ss.str(body);

    string item;
    while (getline(ss, item, '\n'))
    {
        (linesInserter++) = item;
    }

    // iterate over each line (parsing as we go)
    for each (auto line in lines)
    {
        int id = 0;
        string name;
        bool connected = false;

        size_t separator = line.find(',');
        if (separator != std::string::npos)
        {
            // assumption here is that the id of an entry is representable as an int
            id = atoi(&line[separator + 1]);

            name = line.substr(0, separator);
            separator = line.find(',', separator + 1);

            if (separator != std::string::npos)
            {
                connected = atoi(&line[separator + 1]) ? true : false;
            }
        }

        // if we're parsed successfully, update m_peers
        // note: we don't count anything with our id as a peer
        if (name.length() > 0 && id != id_)
        {
            if (connected)
            {
				peers_[id] = name;

                // notify our observer
                RTCWrapAndCall([&, id, name] {
                    if (observer_ != nullptr)
                    {
                        observer_->OnPeerConnected(id, name);
                    }
                });
            }
            else
            {
                // notify our observer
                RTCWrapAndCall([&, id] {
                    if (observer_ != nullptr)
                    {
                        observer_->OnPeerDisconnected(id);
                    }
                });
                
				peers_.erase(id);
            }
        }
    }
}

void PeerConnectionClient::RTCWrapAndCall(const function<void()>& func)
{
    // lock our mutex and release at end of scope
    lock_guard<recursive_mutex> guard(m_wrapAndCallMutex);

    m_signalingThread->Invoke<void>(RTC_FROM_HERE, func);
}