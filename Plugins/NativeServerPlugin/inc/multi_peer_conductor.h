#pragma once

#include "pch.h"

#include <map>
#include <string>
#include <atomic>

#include "peer_connection_client.h"
#include "directx_peer_conductor.h"

#include "webrtc/base/sigslot.h"

using namespace StreamingToolkit;

using namespace std;
using namespace rtc;
using namespace webrtc;
using namespace sigslot;

class MultiPeerConductor : public PeerConnectionClientObserver,
	public MessageHandler,
	public Runnable,
	public has_slots<>
{
public:
	MultiPeerConductor(shared_ptr<WebRTCConfig> config,
		ID3D11Device* d3dDevice,
		bool enableSoftware = false);

	~MultiPeerConductor();

	// Connect the signalling implementation to the signalling server
	void ConnectSignallingAsync(const string& clientName);

	// each peer can emit a signal that will in turn call this method
	void OnIceConnectionChange(int peer_id, PeerConnectionInterface::IceConnectionState new_state);

	virtual void OnSignedIn() override;

	virtual void OnDisconnected() override;

	virtual void OnPeerConnected(int id, const string& name) override;

	virtual void OnPeerDisconnected(int peer_id) override;

	virtual void OnMessageFromPeer(int peer_id, const string& message) override;

	virtual void OnMessageSent(int err) override;

	virtual void OnServerConnectionFailure() override;

	virtual void OnMessage(Message* msg) override;

	virtual void Run(Thread* thread) override;

	const map<int, scoped_refptr<DirectXPeerConductor>>& Peers() const;

private:
	struct MessageEntry
	{
		int peer;
		string message;
		MessageEntry(int p, const string& s) : peer(p), message(s) {}
	};
	
	PeerConnectionClient m_signallingClient;
	shared_ptr<WebRTCConfig> m_webrtcConfig;
	ID3D11Device* m_d3dDevice;
	bool m_enableSoftware;
	scoped_refptr<PeerConnectionFactoryInterface> m_peerFactory;
	map<int, scoped_refptr<DirectXPeerConductor>> m_connectedPeers;
	queue<MessageEntry> m_messageQueue;
	atomic_bool m_shouldProcessQueue;
	unique_ptr<Thread> m_processThread;
};