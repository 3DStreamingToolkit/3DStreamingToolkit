/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_DEFAULT_DATA_CHANNEL_OBSERVER_H_
#define WEBRTC_DEFAULT_DATA_CHANNEL_OBSERVER_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"

class DefaultDataChannelObserver : public webrtc::DataChannelObserver {
public:
	explicit DefaultDataChannelObserver(
		webrtc::DataChannelInterface* channel,
		void (*input_update_func)(const std::string&));

	virtual ~DefaultDataChannelObserver();

	void OnBufferedAmountChange(uint64_t previous_amount) override;

	void OnStateChange() override;
	
	void OnMessage(const webrtc::DataBuffer& buffer) override;

	bool IsOpen() const;
	
	std::vector<std::string> messages() const;
	
	std::string last_message() const;
	
	size_t received_message_count() const;

private:
	rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
	void (*input_update_func_)(const std::string&);
	webrtc::DataChannelInterface::DataState state_;
	std::vector<std::string> messages_;
};

#endif // WEBRTC_DEFAULT_DATA_CHANNEL_OBSERVER_H_
