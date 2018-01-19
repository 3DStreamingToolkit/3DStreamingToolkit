#include "pch.h"
#include "multi_peer_conductor.h"

MultiPeerConductor::MultiPeerConductor(shared_ptr<WebRTCConfig> config,
	ID3D11Device* d3dDevice,
	bool enableSoftware) :
	m_webrtcConfig(config),
	m_d3dDevice(d3dDevice),
	m_enableSoftware(enableSoftware)
{
	m_signallingClient.RegisterObserver(this);
	m_peerFactory = webrtc::CreatePeerConnectionFactory();
	m_processThread = rtc::Thread::Create();
	m_processThread->Start(this);
}

MultiPeerConductor::~MultiPeerConductor()
{
	m_processThread->Quit();
}

void MultiPeerConductor::ConnectSignallingAsync(const string& clientName)
{
	m_signallingClient.Connect(m_webrtcConfig->server, m_webrtcConfig->port, clientName);
}

void MultiPeerConductor::OnIceConnectionChange(int peer_id, PeerConnectionInterface::IceConnectionState new_state)
{
	// peer disconnected
	if (new_state == PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected)
	{
		m_connectedPeers.erase(peer_id);
	}
}

void MultiPeerConductor::OnSignedIn()
{
	m_shouldProcessQueue.store(true);
}

void MultiPeerConductor::OnDisconnected()
{
	m_shouldProcessQueue.store(false);
}

void MultiPeerConductor::OnPeerConnected(int id, const string& name)
{
	m_connectedPeers[id] = new RefCountedObject<DirectXPeerConductor>(id,
		name,
		m_webrtcConfig,
		m_peerFactory,
		[&, id](const string& message)
	{
		m_messageQueue.push(MessageEntry(id, message));
		rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
	},
		m_d3dDevice,
		m_enableSoftware);

	m_connectedPeers[id]->SignalIceConnectionChange.connect(this, &MultiPeerConductor::OnIceConnectionChange);
}

void MultiPeerConductor::OnPeerDisconnected(int peer_id)
{
	m_connectedPeers.erase(peer_id);
}

void MultiPeerConductor::OnMessageFromPeer(int peer_id, const string& message)
{
	m_connectedPeers[peer_id]->HandlePeerMessage(message);
}

void MultiPeerConductor::OnMessageSent(int err)
{
}

void MultiPeerConductor::OnServerConnectionFailure() {}

void MultiPeerConductor::OnMessage(Message* msg)
{
	if (!m_shouldProcessQueue.load() ||
		m_messageQueue.size() == 0)
	{
		return;
	}

	auto peerMessage = m_messageQueue.front();

	if (m_signallingClient.SendToPeer(peerMessage.peer, peerMessage.message))
	{
		m_messageQueue.pop();
	}

	if (m_messageQueue.size() > 0)
	{
		rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, 500, this, 0);
	}
}

void MultiPeerConductor::Run(Thread* thread)
{
	while (!thread->IsQuitting())
	{
		thread->ProcessMessages(500);
	}
}

const map<int, scoped_refptr<DirectXPeerConductor>>& MultiPeerConductor::Peers() const
{
	return m_connectedPeers;
}
