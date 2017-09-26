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

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "defaults.h"
#include "webrtc/base/arraysize.h"

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";
const uint16_t kDefaultServerPort = 3000;
const int kDefaultHeartbeat = -1; // this means, by default, don't use heartbeats

std::string GetEnvVarOrDefault(const char* env_var_name, const char* default_value)
{
	std::string value;
	const char* env_var = getenv(env_var_name);
	if (env_var)
	{
		value = env_var;
	}

	if (value.empty())
	{
		value = default_value;
	}

	return value;
}

std::string GetPeerConnectionString()
{
	return GetEnvVarOrDefault("WEBRTC_CONNECT", "stun:stun.l.google.com:19302");
}

std::string GetDefaultServerName()
{
	return GetEnvVarOrDefault("WEBRTC_SERVER", "signalingserveruri");
}

std::string GetPeerName()
{
	char computer_name[256];
	std::string ret(GetEnvVarOrDefault("USERNAME", "user"));
	ret += '@';
	if (gethostname(computer_name, arraysize(computer_name)) == 0)
	{
		ret += computer_name;
	}
	else
	{
		ret += "host";
	}

	return ret;
}
