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
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnSignalingChange(new_state); });
}

void PeerConnectionMultiObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnAddStream(stream); });
}

void PeerConnectionMultiObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnRemoveStream(stream); });
}

void PeerConnectionMultiObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnDataChannel(channel); });
}

void PeerConnectionMultiObserver::OnRenegotiationNeeded()
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnRenegotiationNeeded(); });
}

void PeerConnectionMultiObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnIceConnectionChange(new_state); });
}

void PeerConnectionMultiObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnIceGatheringChange(new_state); });
}

void PeerConnectionMultiObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnIceCandidate(candidate); });
}

void PeerConnectionMultiObserver::OnIceConnectionReceivingChange(bool receiving)
{
	std::for_each(observers_.rbegin(), observers_.rend(), [&](auto o) { o->OnIceConnectionReceivingChange(receiving); });
}