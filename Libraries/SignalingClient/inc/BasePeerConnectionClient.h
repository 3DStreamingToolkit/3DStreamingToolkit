#pragma once

#include <string>
#include <map>

#include <webrtc/base/messagehandler.h>
#include <webrtc/base/sigslot.h>

#include "PeerConnectionClientObserver.h"

using namespace std;

namespace SignalingClient
{
	/// <summary>
	/// Base class that all imeplementations of PeerConnectionClient for use with 3DToolkit should derive from
	/// </summary>
	/// <remarks>
	/// observer_.OnPeerConnected() and observer_.OnPeerDisconnected() will be triggered intermittenly
	/// </remarks>
	class BasePeerConnectionClient : public sigslot::has_slots<>,
		public rtc::MessageHandler
	{
	public:
		/// <summary>
		/// The default id for freshly initialized clients
		/// </summary>
		constexpr static int DefaultId = -1;

		/// <summary>
		/// The bye message that a client sends on disconnect
		/// </summary>
		constexpr static const char ByeMessage[] = "BYE";

		/// <summary>
		/// Default dtor
		/// </summary>
		virtual ~BasePeerConnectionClient() {}

		/// <summary>
		/// Connects to a given hostname:port as the specified clientName
		/// </summary>
		/// <param name="hostname">the remote host</param>
		/// <param name="port">the remote port</param>
		/// <param name="clientName">the client name to identify as</param>
		/// <remarks>
		/// This will trigger observer_.OnSignedIn() or observer_.OnServerConnectionFailure()
		/// </remarks>
		virtual void SignIn(string hostname, uint32_t port, string clientName) = 0;

		/// <summary>
		/// Disconnects from a remote, if currently connected
		/// </summary>
		/// <remarks>
		/// This will trigger observer_.OnDisconnected()
		/// </remarks>
		virtual void SignOut() = 0;

		/// <summary>
		/// Proxies all requests through the given proxy uri
		/// </summary>
		/// <param name="proxy">fully qualified proxy uri to use</param>
		virtual void SetProxy(const string& proxy) = 0;

		/// <summary>
		/// Sends data to the peer identified by clientId
		/// </summary>
		/// <param name="clientId">the clientId to send to</param>
		/// <param name="data">the data to send</param>
		/// <remarks>
		/// This will trigger observer_.OnMessageSent()
		/// </remarks>
		virtual void SendToPeer(int clientId, string data) = 0;

		/// <summary>
		/// Sends a specific signal indicating we wish to cease communication to the peer identified by clientId
		/// </summary>
		/// <param name="clientId">the clientId to send to</param>
		/// <remarks>
		/// This will trigger observer_.OnMessageSent()
		/// </remarks>
		virtual void SendHangUp(int clientId) = 0;

		/// <summary>
		/// Set the observer that will receive notifications about events that occr
		/// </summary>
		/// <param name="observer">the observer</param>
		void RegisterObserver(PeerConnectionClientObserver* observer) { observer_ = observer; }

		/// <summary>
		/// Get the peers that are connected
		/// </summary>
		const map<int, string> peers() { return peers_; }

		/// <summary>
		/// Get the client id
		/// </summary>
		const int id() { return id_; }

		/// <summary>
		/// Get the client name
		/// </summary>
		const string name() { return name_; }

	protected:
		/// <summary>
		/// Default ctor, only visible to derived classes
		/// </summary>
		BasePeerConnectionClient() :
			id_(DefaultId),
			observer_(nullptr)
			{}

		/// <summary>
		/// internal representation of the connected peers
		/// </summary>
		/// <remarks>
		/// As an implementor, some internal mechanism should keep this
		/// up to date regularly (and trigger observer_.OnPeerConnected() and observer_.OnPeerDisconnected() as needed
		/// </remarks>
		map<int, string> peers_;

		/// <summary>
		/// internal representation of the client id
		/// </summary>
		/// <remarks>
		/// As an implementor, SignIn should set this, and SignOut should unset this
		/// </remarks>
		int id_;

		/// <summary>
		/// internal representation of the client name
		/// </summary>
		/// <remarks>
		/// As an implementor, SignIn should set this, and SignOut should unset this
		/// </remarks>
		string name_;

		/// <summary>
		/// internal representation of the observer
		/// </summary>
		PeerConnectionClientObserver* observer_;
	};
}