#pragma once

#include <vector>
#include "webrtc/api/peerconnectioninterface.h"

/// <summary>
/// A lightweight PeerConnectionObserver implementation that acts as a proxy
/// to a vector of many other PeerConnectionObservers
/// </summary>
class PeerConnectionMultiObserver : public webrtc::PeerConnectionObserver
{
public:
	PeerConnectionMultiObserver(const std::vector<webrtc::PeerConnectionObserver*>& observers);

	~PeerConnectionMultiObserver();

	void OnSignalingChange(
		webrtc::PeerConnectionInterface::SignalingState new_state) override;

	void OnAddStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;

	void OnRemoveStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;

	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

	void OnRenegotiationNeeded() override;

	void OnIceConnectionChange(
		webrtc::PeerConnectionInterface::IceConnectionState new_state) override;

	void OnIceGatheringChange(
		webrtc::PeerConnectionInterface::IceGatheringState new_state) override;

	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

	void OnIceConnectionReceivingChange(bool receiving) override;
private:
	std::vector<webrtc::PeerConnectionObserver*> observers_;
};

