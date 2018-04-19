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
#include <functional>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/win32.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"

class MainWindowCallback
{
public:
	virtual void StartLogin(const std::string& server, int port) = 0;

	virtual void DisconnectFromServer() = 0;

	virtual void ConnectToPeer(int peer_id) = 0;

	virtual void DisconnectFromCurrentPeer() = 0;

	virtual void UIThreadCallback(int msg_id, void* data) = 0;

	virtual void Close() = 0;

protected:
	virtual ~MainWindowCallback() {}
};

class ThreadSafeMainWindowCallback : public rtc::MessageHandler, public MainWindowCallback
{
public:
	ThreadSafeMainWindowCallback(MainWindowCallback* wrap, rtc::Thread* thread = nullptr) :
		wrap_(wrap)
	{
		// thread or rtc::Thread::Current or WrapCurrentThread() depending on existence
		captured_thread_ = thread == nullptr ?
			rtc::Thread::Current() == nullptr ?
			rtc::ThreadManager::Instance()->WrapCurrentThread()
			: rtc::Thread::Current()
			: thread;
	}

	virtual void OnMessage(rtc::Message* msg) override
	{
		// note: some cases use braces, where initialization occurs inline
		// see https://stackoverflow.com/questions/5136295/switch-transfer-of-control-bypasses-initialization-of-when-calling-a-function
		switch (msg->message_id)
		{
			case MessageType::START_LOGIN:
			{
				auto slDataW = (rtc::TypedMessageData<StartLoginData>*)msg->pdata;
				auto slData = slDataW->data();
				wrap_->StartLogin(slData.server, slData.port);
				break;
			}
			case MessageType::DISCONNECT_FROM_SERVER:
				wrap_->DisconnectFromServer();
				break;
			case MessageType::CONNECT_TO_PEER:
			{
				auto ctpDataW = (rtc::TypedMessageData<ConnectToPeerData>*)msg->pdata;
				auto ctpData = ctpDataW->data();
				wrap_->ConnectToPeer(ctpData.peer);
				break;
			}
			case MessageType::DISCONNECT_FROM_PEER:
				wrap_->DisconnectFromCurrentPeer();
				break;
			case MessageType::UI_THREAD_CALLBACK:
			{
				auto uiDataW = (rtc::TypedMessageData<UiThreadCallbackData>*)msg->pdata;
				auto uiData = uiDataW->data();
				wrap_->UIThreadCallback(uiData.msgId, uiData.data);
				break;
			}
			case MessageType::CLOSE:
				wrap_->Close();
				break;
			default:
				// no need to execute the following logic if we're not handling one of our messages
				return;
		}

		// this was allocated just for us, time to delete
		if (msg->pdata != nullptr)
		{
			delete msg->pdata;
		}
	}

	virtual void StartLogin(const std::string& server, int port) override
	{
		StartLoginData data;
		data.server = server;
		data.port = port;

		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::START_LOGIN,
			new rtc::TypedMessageData<StartLoginData>(data));
	}

	virtual void DisconnectFromServer() override
	{
		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::DISCONNECT_FROM_SERVER,
			nullptr);
	}

	virtual void ConnectToPeer(int peer_id) override
	{
		ConnectToPeerData data;
		data.peer = peer_id;

		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::CONNECT_TO_PEER,
			new rtc::TypedMessageData<ConnectToPeerData>(data));
	}

	virtual void DisconnectFromCurrentPeer() override
	{
		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::DISCONNECT_FROM_PEER,
			nullptr);
	}

	virtual void UIThreadCallback(int msg_id, void* data) override
	{
		UiThreadCallbackData postData;
		postData.msgId = msg_id;
		postData.data = data;

		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::UI_THREAD_CALLBACK,
			new rtc::TypedMessageData<UiThreadCallbackData>(postData));
	}

	virtual void Close() override
	{
		captured_thread_->Post(RTC_FROM_HERE,
			this,
			MessageType::CLOSE,
			nullptr);
	}
private:
	enum MessageType
	{
		NONE = 0,
		START_LOGIN,
		DISCONNECT_FROM_SERVER,
		CONNECT_TO_PEER,
		DISCONNECT_FROM_PEER,
		UI_THREAD_CALLBACK,
		CLOSE
	};

	struct StartLoginData { std::string server; int port; };
	struct ConnectToPeerData { int peer; };
	struct UiThreadCallbackData { int msgId; void* data; };

	MainWindowCallback* wrap_;
	rtc::Thread* captured_thread_;
};

class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
	virtual void Lock() = 0;
	virtual void Unlock() = 0;

	virtual void OnFrame(const webrtc::VideoFrame& frame) override = 0;

	virtual const BITMAPINFO& bmi() const = 0;
	virtual const uint8_t* image() const = 0;
};

class MainWindow
{
public:
	static const wchar_t kClassName[];
	static const std::map<int, std::string> kEmptyPeersMap;

	enum UI
	{
		CONNECT_TO_SERVER,
		LIST_PEERS,
		STREAMING,
	};

	enum WindowMessages
	{
		UI_THREAD_CALLBACK = WM_APP + 1,
	};

	typedef std::function<VideoRenderer*(HWND, int, int, webrtc::VideoTrackInterface*)> VideoRendererAllocator;

	MainWindow(VideoRendererAllocator videoRendererAllocator);

	~MainWindow();

	sigslot::signal1<MSG*> SignalDataChannelMessage;

	// called OnMessage, with the given values
	// msg, lp, wp, result, retCode (of OnMessage)
	sigslot::signal5<UINT, WPARAM, LPARAM, LRESULT*, bool*> SignalWindowMessage;

	void RegisterObserver(MainWindowCallback* callback);

	bool IsWindow();

	void MessageBox(const char* caption, const char* text, bool is_error);

	UI current_ui();

	HWND handle();

	void SwitchToConnectUI();

	void SwitchToPeerList(const std::map<int, std::string>& peers);

	void SwitchToStreamingUI();

	void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);

	void StopLocalRenderer();

	void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);

	void StopRemoteRenderer();

	void QueueUIThreadCallback(int msg_id, void* data);

	void OnDestroyed();

	bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	
	static bool RegisterWindowClass();

	// must be implemented by a child
	virtual bool Create() = 0;
	virtual bool Destroy() = 0;
	virtual void SetAuthCode(const std::wstring& str) = 0;
	virtual void SetAuthUri(const std::wstring& str) = 0;
	virtual void LayoutConnectUI(bool visible) = 0;
	virtual void LayoutPeerListUI(const std::map<int, std::string>& peers, bool visible) = 0;
	virtual void OnDefaultAction() = 0;
	virtual void OnPaint() = 0;

protected:
	HWND wnd_;
	UI current_ui_;
	DWORD ui_thread_id_;
	std::vector<MainWindowCallback*> callbacks_;
	std::unique_ptr<VideoRenderer> local_video_renderer_;
	std::unique_ptr<VideoRenderer> remote_video_renderer_;

private:
	bool destroyed_;
	void* nested_msg_;
	VideoRendererAllocator video_renderer_alloc_;
	static ATOM wnd_class_;
};

#endif // WEBRTC_MAIN_WINDOW_H_
