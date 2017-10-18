#include "peer_connection_multi_observer.h"

PeerConnectionMultiObserver::PeerConnectionMultiObserver(const std::vector<webrtc::PeerConnectionObserver*>& observers) :
	observers_(observers)
{
}

PeerConnectionMultiObserver::~PeerConnectionMultiObserver()
{
}

void PeerConnectionMultiObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnSignalingChange(new_state);
	}
}

void PeerConnectionMultiObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnAddStream(stream);
	}
}

void PeerConnectionMultiObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnRemoveStream(stream);
	}
}

void PeerConnectionMultiObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnDataChannel(channel);
	}
}

void PeerConnectionMultiObserver::OnRenegotiationNeeded()
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnRenegotiationNeeded();
	}
}

void PeerConnectionMultiObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnIceConnectionChange(new_state);
	}
}

void PeerConnectionMultiObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnIceGatheringChange(new_state);
	}
}

void PeerConnectionMultiObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnIceCandidate(candidate);
	}
}

void PeerConnectionMultiObserver::OnIceConnectionReceivingChange(bool receiving)
{
	for (auto it = observers_.rbegin(); it != observers_.rend(); it++)
	{
		(*it)->OnIceConnectionReceivingChange(receiving);
	}
}