/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MAIN_WINDOW_H_
#define WEBRTC_MAIN_WINDOW_H_

#include <map>
#include <memory>
#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/win32.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"
#include "peer_connection_client.h"

class MainWindowCallback
{
public:
	virtual void StartLogin(const std::string& server, int port) = 0;

	virtual void DisconnectFromServer() = 0;

	virtual void ConnectToPeer(int peer_id) = 0;

	virtual void DisconnectFromCurrentPeer() = 0;

	virtual void ProcessInput(const std::string& message) = 0;

	virtual void UIThreadCallback(int msg_id, void* data) = 0;

	virtual void Close() = 0;

protected:
	virtual ~MainWindowCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow
{
public:
	enum UI
	{
		CONNECT_TO_SERVER,
		LIST_PEERS,
		STREAMING,
	};

	virtual ~MainWindow() {}

	virtual void RegisterObserver(MainWindowCallback* callback) = 0;

	virtual bool IsWindow() = 0;

	virtual void MessageBox(const char* caption, const char* text, bool is_error) = 0;

	virtual UI current_ui() = 0;

	virtual void SwitchToConnectUI() = 0;

	virtual void SwitchToPeerList(const Peers& peers) = 0;

	virtual void SwitchToStreamingUI() = 0;

	virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;

	virtual void StopLocalRenderer() = 0;

	virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) = 0;

	virtual void StopRemoteRenderer() = 0;

	virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

#endif // WEBRTC_MAIN_WINDOW_H_
