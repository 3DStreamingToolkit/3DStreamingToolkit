#pragma once

#include "pch.h"

#include <string>
#include <vector>

#include "buffer_capturer.h"

// from ConfigParser
#include "structs.h"

#include "webrtc/base/sigslot.h"
#include "webrtc/base/json.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeconstraints.h"

using namespace std;
using namespace webrtc;
using namespace rtc;
using namespace StreamingToolkit;
using namespace sigslot;

// Abstract PeerConductor
class PeerConductor : public PeerConnectionObserver,
	public CreateSessionDescriptionObserver,
	public DataChannelObserver
{
public:
	PeerConductor(int id,
		const string& name,
		shared_ptr<WebRTCConfig> webrtc_config,
		scoped_refptr<PeerConnectionFactoryInterface> peer_factory,
		const function<void(const string&)>& send_func);

	~PeerConductor();

	// m_id, new_state, threadsafe per-instance 
	signal2<int, PeerConnectionInterface::IceConnectionState, sigslot::multi_threaded_local> SignalIceConnectionChange;

	// This callback transfers the ownership of the |desc|.
	// TODO(deadbeef): Make this take an std::unique_ptr<> to avoid confusion
	// around ownership.
	virtual void OnSuccess(SessionDescriptionInterface* desc) override;

	virtual void OnFailure(const string& error) override;

	// Triggered when the SignalingState changed.
	virtual void OnSignalingChange(
		PeerConnectionInterface::SignalingState new_state) override;

	// Triggered when renegotiation is needed. For example, an ICE restart
	// has begun.
	virtual void OnRenegotiationNeeded() override;

	// Called any time the IceConnectionState changes.
	//
	// Note that our ICE states lag behind the standard slightly. The most
	// notable differences include the fact that "failed" occurs after 15
	// seconds, not 30, and this actually represents a combination ICE + DTLS
	// state, so it may be "failed" if DTLS fails while ICE succeeds.
	virtual void OnIceConnectionChange(
		PeerConnectionInterface::IceConnectionState new_state) override;

	// Called any time the IceGatheringState changes.
	virtual void OnIceGatheringChange(
		PeerConnectionInterface::IceGatheringState new_state) override;

	// A new ICE candidate has been gathered.
	virtual void OnIceCandidate(const IceCandidateInterface* candidate) override;

	void OnAddStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;

	void OnRemoveStream(
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;

	void OnDataChannel(
		rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

	// Emitted when a message is received via data channel
	signal2<int, const string&> SignalDataChannelMessage;

	//  A data buffer was successfully received.
	virtual void OnMessage(const DataBuffer& buffer) override;

	virtual void OnStateChange() override;

	void AllocatePeerConnection(bool create_offer = false);

	void HandlePeerMessage(const string& message);

	const bool IsConnected() const;

	const int Id() const;

	const string& Name() const;

	const vector<scoped_refptr<webrtc::MediaStreamInterface>> Streams() const;

protected:
	// Allocates a buffer capturer for a single video track
	virtual unique_ptr<cricket::VideoCapturer> AllocateVideoCapturer() = 0;

private:
	int id_;
	string name_;
	shared_ptr<WebRTCConfig> webrtc_config_;
	scoped_refptr<PeerConnectionFactoryInterface> peer_factory_;
	function<void(const string&)> send_func_;
	scoped_refptr<PeerConnectionInterface> peer_connection_;
	vector<scoped_refptr<webrtc::MediaStreamInterface>> peer_streams_;

	// Names used for a IceCandidate JSON object.
	const char* kCandidateSdpMidName = "sdpMid";
	const char* kCandidateSdpMlineIndexName = "sdpMLineIndex";
	const char* kCandidateSdpName = "candidate";

	// Names used for stream labels.
	const char* kAudioLabel = "audio_label";
	const char* kVideoLabel = "video_label";
	const char* kStreamLabel = "stream_label";

	// Names used for a SessionDescription JSON object.
	const char* kSessionDescriptionTypeName = "type";
	const char* kSessionDescriptionSdpName = "sdp";
};
