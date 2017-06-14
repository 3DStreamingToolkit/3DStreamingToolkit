#include "peer_connection_client.h"

PeerConnectionClient::PeerConnectionClient() :
    m_id(DefaultId),
    m_allowPolling(true),
    m_observer(nullptr)
{
    // we need a "signaling_thread" so that all our webrtc calls occur
    // on the same rtc::Thread
    auto wrapped = rtc::Thread::Current();

    if (wrapped == nullptr)
    {
        wrapped = ThreadManager::Instance()->WrapCurrentThread();
    }

    m_signalingThread = wrapped;
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
}

void PeerConnectionClient::SignIn(string hostname, uint32_t port, string clientName)
{
    if (m_id != DefaultId)
    {
        return;
    }

    uri_builder baseUriBuilder;
    baseUriBuilder.set_host(conversions::to_utf16string(hostname));
    baseUriBuilder.set_port(port);

    m_controlClient.SetUri(baseUriBuilder.to_uri());
    m_pollingClient.SetUri(baseUriBuilder.to_uri());

    auto existingPollingConfig = m_pollingClient.config();
    existingPollingConfig.set_timeout(utility::seconds::max());

    m_name = clientName;

    uri_builder endpointUriBuilder;
    endpointUriBuilder.append_path(L"/sign_in");
    endpointUriBuilder.append_query(utility::conversions::to_string_t(clientName));

    m_signInChain = m_controlClient.Issue(methods::GET, endpointUriBuilder.to_string())
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
        m_id = atoi(box.pragma.c_str());

        UpdatePeers(box.body);
    })
        .then([&]
    {
        // notify our observer
        RTCWrapAndCall([&] {
            if (m_observer != nullptr)
            {
                m_observer->OnSignedIn();
            }
        });

        // issue a polling request (will self-sustain)
        Poll();
    });
}

void PeerConnectionClient::SignOut()
{
    if (m_id == DefaultId)
    {
        return;
    }

    uri_builder builder;
    builder.append_path(L"sign_out");
    builder.append_query(L"peer_id", std::to_wstring(m_id));

    m_signOutChain = m_controlClient.Issue(methods::GET, builder.to_string())
        .then([&](http_response) { m_id = -1; })
        .then([&]
    {
        m_controlClient.CancelAll();
        m_pollingClient.CancelAll();

        m_peers.clear();

        // notify our observer
        RTCWrapAndCall([&] {
            if (m_observer != nullptr)
            {
                m_observer->OnDisconnected();
            }
        });
    });
}

void PeerConnectionClient::set_proxy(const string& proxy)
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

const map<int, string> PeerConnectionClient::peers()
{
    return m_peers;
}

const int PeerConnectionClient::id()
{
    return m_id;
}

const string PeerConnectionClient::name()
{
    return m_name;
}

void PeerConnectionClient::SendToPeer(int clientId, string data)
{
    if (m_id == DefaultId)
    {
        return;
    }

    uri_builder builder;
    builder.append_path(L"message");
    builder.append_query(L"peer_id", std::to_wstring(m_id));
    builder.append_query(L"to", std::to_wstring(clientId));

    m_sendToPeerChain = m_controlClient.Issue(methods::POST, builder.to_string(), utility::conversions::to_string_t(data))
        .then([&](http_response res)
    {
        auto status = res.status_code();

        // notify our observer
        RTCWrapAndCall([&, status] {
            if (m_observer != nullptr)
            {
                m_observer->OnMessageSent(status);
            }
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
        m_name.length() <= 0)
    {
        // ERROR, default initialized means we can't "retry"
        return;
    }

    // use our existing uri values to reconnect
    auto uri = m_controlClient.uri();
    auto hostAsStr = utility::conversions::to_utf8string(uri.host());

    // try connecting
    SignIn(hostAsStr, uri.port(), m_name);
}

void PeerConnectionClient::RegisterObserver(Observer* observer)
{
    m_observer = observer;
}

void PeerConnectionClient::Poll()
{
    if (!m_allowPolling)
    {
        return;
    }

    uri_builder builder;
    builder.append_path(L"wait");
    builder.append_query(L"peer_id", std::to_wstring(m_id));

    m_pollChain = m_pollingClient.Issue(methods::GET, builder.to_string())
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

        if (senderId == m_id)
        {
            // note: we notify the observer of peer changes INSIDE UpdatePeers()
            UpdatePeers(body);
        }
        else
        {
            // notify our observer
            RTCWrapAndCall([&, senderId, body] {
                if (m_observer != nullptr)
                {
                    m_observer->OnMessageFromPeer(senderId, body);
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
        if (name.length() > 0 && id != m_id)
        {
            if (connected)
            {
                m_peers[id] = name;

                // notify our observer
                RTCWrapAndCall([&, id, name] {
                    if (m_observer != nullptr)
                    {
                        m_observer->OnPeerConnected(id, name);
                    }
                });
            }
            else
            {
                // notify our observer
                RTCWrapAndCall([&, id] {
                    if (m_observer != nullptr)
                    {
                        m_observer->OnPeerDisconnected(id);
                    }
                });
                
                m_peers.erase(id);
            }
        }
    }
}

void PeerConnectionClient::RTCWrapAndCall(const function<void()>& func)
{
    // lock our mutex and release at end of scope
    lock_guard<mutex> guard(m_wrapAndCallMutex);

    m_signalingThread->Invoke<void>(RTC_FROM_HERE, func);
}