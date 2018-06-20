#pragma once

#include "peer_connection_client.h"

using namespace std;

class NoFailureObserver : public PeerConnectionClientObserver
{
public:
	NoFailureObserver() : evt(true, false) {}

	void OnSignedIn() override
	{
		status = true;
		evt.Set();
	}

	void OnDisconnected() override
	{
		status = true;
		evt.Set();
	}

	void OnPeerConnected(int id, const string& name) override {}

	void OnPeerDisconnected(int peer_id) override {}

	void OnMessageFromPeer(int peer_id, const string& message) override {}

	void OnMessageSent(int err) override {}

	void OnHeartbeat(int heartbeat_status) override {}

	void OnServerConnectionFailure() override
	{
		// this is the only failure case
		status = false;
		evt.Set();
	}

	bool Wait()
	{
		return evt.Wait(rtc::Event::kForever) && status;
	}
private:
	bool status;
	rtc::Event evt;
};
