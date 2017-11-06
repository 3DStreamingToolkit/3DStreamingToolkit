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

#include "input_data_channel_observer.h"

using namespace StreamingToolkit;

InputDataChannelObserver::InputDataChannelObserver(
	webrtc::DataChannelInterface* channel, InputDataHandler* handler) :
		channel_(channel), handler_(handler)
{
	channel_->RegisterObserver(this);
	state_ = channel_->state();
}

InputDataChannelObserver::~InputDataChannelObserver()
{
	channel_->UnregisterObserver();
}

void InputDataChannelObserver::OnBufferedAmountChange(uint64_t previous_amount)
{
}

void InputDataChannelObserver::OnStateChange()
{ 
	state_ = channel_->state();
}

void InputDataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer)
{
	if (handler_)
	{
		handler_->Handle(std::string((const char*)buffer.data.data(), buffer.data.size()));
	}
}

bool InputDataChannelObserver::IsOpen() const
{ 
	return state_ == webrtc::DataChannelInterface::kOpen;
}

std::vector<std::string> InputDataChannelObserver::messages() const
{ 
	return messages_; 
}

std::string InputDataChannelObserver::last_message() const
{
	return messages_.empty() ? std::string() : messages_.back();
}

size_t InputDataChannelObserver::received_message_count() const
{ 
	return messages_.size(); 
}
