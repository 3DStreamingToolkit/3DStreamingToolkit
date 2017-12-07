#pragma once

#include "pch.h"

#include <map>
#include <string>

#include "peer_connection_client.h"
#include "peer_conductor.hpp"

using namespace StreamingToolkit;

using namespace std;
using namespace rtc;
using namespace webrtc;

class MultiPeerConductor : public PeerConnectionClientObserver
{
public:
	MultiPeerConductor(shared_ptr<WebRTCConfig> config,
		ID3D11Device* d3dDevice,
		bool enableSoftware = false) :
		m_webrtcConfig(config),
		m_d3dDevice(d3dDevice),
		m_enableSoftware(enableSoftware),
		m_signallingClient("")
	{
		m_signallingClient.RegisterObserver(this);
		m_peerFactory = webrtc::CreatePeerConnectionFactory();
	}

	// Connect the signalling implementation to the signalling server
	void ConnectSignallingAsync(const string& clientName)
	{
		m_signallingClient.Connect(m_webrtcConfig->server, m_webrtcConfig->port, clientName);
	}

	virtual void OnSignedIn() override {}  // Called when we're logged on.

	virtual void OnDisconnected() override {}

	virtual void OnPeerConnected(int id, const string& name) override
	{
		/*
		
		int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtcConfig,
		scoped_refptr<PeerConnectionFactoryInterface> peerFactory,
		const function<void(const string&)>& sendFunc,
		ID3D11Device* d3dDevice
		bool enableSoftware

		*/
		m_connectedPeers[id] = new RefCountedObject<DirectXPeerConductor>(id,
			name,
			m_webrtcConfig,
			m_peerFactory,
			[&, id](const string& message) { m_signallingClient.SendToPeer(id, message); },
			m_d3dDevice,
			m_enableSoftware);
	}

	virtual void OnPeerDisconnected(int peer_id) override
	{
		m_connectedPeers.erase(peer_id);
	}

	virtual void OnMessageFromPeer(int peer_id, const string& message) override
	{
		m_connectedPeers[peer_id]->HandlePeerMessage(message);
	}

	virtual void OnMessageSent(int err) override {}

	virtual void OnServerConnectionFailure() override {}

	const map<int, scoped_refptr<DirectXPeerConductor>>& Peers() const
	{
		return m_connectedPeers;
	}

private:
	PeerConnectionClient m_signallingClient;
	shared_ptr<WebRTCConfig> m_webrtcConfig;
	ID3D11Device* m_d3dDevice;
	bool m_enableSoftware;
	scoped_refptr<PeerConnectionFactoryInterface> m_peerFactory;
	map<int, scoped_refptr<DirectXPeerConductor>> m_connectedPeers;
};