#pragma once

#include <string>

/// <summary>
/// structure capable of receiving event notifications from an implementation of <see cref="BasePeerConnectionClient" />
/// </summary>
struct PeerConnectionClientObserver
{
public:
	/// <summary>
	/// Triggered when sign in occurs successfully
	/// </summary>
	virtual void OnSignedIn() = 0;

	/// <summary>
	/// Triggered when disconnection occurs
	/// </summary>
	virtual void OnDisconnected() = 0;

	/// <summary>
	/// Triggered when a new peer is connected
	/// </summary>
	/// <param name="id">the new peer's id</param>
	/// <param name="name">the new peer's name</param>
	virtual void OnPeerConnected(int id, const std::string& name) = 0;

	/// <summary>
	/// Triggered when a peer disconnects
	/// </summary>
	/// <param name="id">the peer's id</param>
	virtual void OnPeerDisconnected(int id) = 0;

	/// <summary>
	/// Triggered when a peers sends a message
	/// </summary>
	/// <param name="id">the peer's id</param>
	/// <param name="message">the peer's message</param>
	virtual void OnMessageFromPeer(int id, const std::string& message) = 0;

	/// <summary>
	/// Triggered when a message is sent to a peer
	/// </summary>
	/// <param name="status">http status code of the operation</param>
	virtual void OnMessageSent(int status) = 0;

	/// <summary>
	/// Triggered when we fail to SignIn due to connection issues
	/// </summary>
	virtual void OnServerConnectionFailure() = 0;

protected:
	/// <summary>
	/// Default dtor
	/// </summary>
	virtual ~PeerConnectionClientObserver() {}
};