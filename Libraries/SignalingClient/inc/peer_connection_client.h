#pragma once

#include <webrtc/base/thread.h>

#include "BasePeerConnectionClient.h"
#include "OrderedHttpClient.h"

using namespace std;
using namespace concurrency;
using namespace SignalingClient;
using namespace SignalingClient::Http;

/// <summary>
/// PeerConnectionClient implementation v1
/// </summary>
class PeerConnectionClient : public BasePeerConnectionClient
{
public:
	/// <summary>
	/// Default ctor
	/// </summary>
    PeerConnectionClient();

	/// <summary>
	/// Default dtor
	/// </summary>
    ~PeerConnectionClient();

	virtual void SignIn(string hostname, uint32_t port, string clientName);

	virtual void SignOut();

	virtual void SetProxy(const string& proxy);

	virtual void SendToPeer(int clientId, string data);

	virtual void SendHangUp(int clientId);

	virtual void OnMessage(rtc::Message* msg);

	/// <summary>
	/// Test hook to provide access to the underlying signaling thread
	/// </summary>
	rtc::Thread* signaling_thread();

private:
	/// <summary>
	/// Internal polling method that handles constant polling for messages
	/// and peer updates from the signaling server
	/// </summary>
    void Poll();

	/// <summary>
	/// Helper method that parses peer data from an http response body
	/// and updates peers_ accordingly
	/// </summary>
	/// <param name="body">the http response body to parse</param>
    void UpdatePeers(string body);

	/// <summary>
	/// Helper method to wrap a function and invoke it on
	/// our signaling thread
	/// </summary>
	/// <param name="func">the function to invoke</param>
	/// <remarks>
	/// This is necessary for webrtc logic that's triggered
	/// inside observer_ callbacks
	/// </remarks>
    void RTCWrapAndCall(const function<void()>& func);

	/// <summary>
	/// the http client we use to make requests related to signaling
	/// </summary>
    OrderedHttpClient m_controlClient;

	/// <summary>
	/// the http client we use to make a single long poll hanging request
	/// </summary>
    OrderedHttpClient m_pollingClient;

	/// <summary>
	/// The task representing pending sign in operations
	/// </summary>
    task<void> m_signInChain;

	/// <summary>
	/// The task representing pending sign out operations
	/// </summary>
    task<void> m_signOutChain;

	/// <summary>
	/// The task representing pending send to peer operations
	/// </summary>
    task<void> m_sendToPeerChain;

	/// <summary>
	/// The task representing pending polling operations
	/// </summary>
    task<void> m_pollChain;

	/// <summary>
	/// Flag that when true, allows polling to continue
	/// </summary>
	/// <remarks>
	/// Setting this to false cancels future polling
	/// </remarks>
	bool m_allowPolling;

	/// <summary>
	/// Mutex used to ensure we only call one observer_ callback at a time
	/// </summary>
    mutex m_wrapAndCallMutex;

	/// <summary>
	/// Pointer to the thread we use for signaling
	/// </summary>
    rtc::Thread* m_signalingThread;

	/// <summary>
	/// Internally represents both an http response body, and it's pragma header
	/// </summary>
    struct BodyAndPragma
    {
		/// <summary>
		/// the body of an http response
		/// </summary>
        string body;

		/// <summary>
		/// the pragma header of an http response
		/// </summary>
        string pragma;
    };
};