/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pch.h"

#include "default_data_channel_observer.h"

DefaultDataChannelObserver::DefaultDataChannelObserver(
	webrtc::DataChannelInterface* channel,
	void (*input_update_func)(const std::string&)) : 
		channel_(channel),
		input_update_func_(input_update_func)
{
	channel_->RegisterObserver(this);
	state_ = channel_->state();
}

DefaultDataChannelObserver::~DefaultDataChannelObserver() 
{
	channel_->UnregisterObserver();
}

void DefaultDataChannelObserver::OnBufferedAmountChange(uint64_t previous_amount)
{
}

void DefaultDataChannelObserver::OnStateChange()
{ 
	state_ = channel_->state();
}

void DefaultDataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer) 
{
	input_update_func_(std::string((const char*)buffer.data.data(), buffer.data.size()));
}

bool DefaultDataChannelObserver::IsOpen() const
{ 
	return state_ == webrtc::DataChannelInterface::kOpen;
}

std::vector<std::string> DefaultDataChannelObserver::messages() const 
{ 
	return messages_; 
}

std::string DefaultDataChannelObserver::last_message() const
{
	return messages_.empty() ? std::string() : messages_.back();
}

size_t DefaultDataChannelObserver::received_message_count() const 
{ 
	return messages_.size(); 
}
