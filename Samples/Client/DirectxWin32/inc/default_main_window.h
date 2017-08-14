/*
*  Copyright 2012 The WebRTC Project Authors. All rights reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_DEFAULT_MAIN_WINDOW_H_
#define WEBRTC_DEFAULT_MAIN_WINDOW_H_

#include <map>
#include <memory>
#include <string>

#include "defs.h"
#include "main_window.h"
#include "peer_connection_client.h"
#include "win32_data_channel_handler.h"

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/win32.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"

class DefaultMainWindow : public MainWindow 
{
public:
	static const wchar_t kClassName[];

	enum WindowMessages 
	{
		UI_THREAD_CALLBACK = WM_APP + 1,
	};

	DefaultMainWindow(
		const char* server,
		int port,
		bool auto_connect,
		bool auto_call,
		bool has_no_UI = false,
		int width = CW_USEDEFAULT,
		int height = CW_USEDEFAULT);

	~DefaultMainWindow();

	bool Create();

	bool Destroy();

	bool PreTranslateMessage(MSG* msg);

	virtual void RegisterObserver(MainWindowCallback* callback);

	virtual bool IsWindow();

	virtual void SwitchToConnectUI();

	virtual void SwitchToPeerList(const Peers& peers);

	virtual void SwitchToStreamingUI();

	virtual void MessageBox(const char* caption, const char* text,
		bool is_error);

	virtual UI current_ui() { return ui_; }

	virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);

	virtual void StopLocalRenderer();

	virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);

	virtual void StopRemoteRenderer();

	virtual void QueueUIThreadCallback(int msg_id, void* data);

	HWND handle() const
	{
		return wnd_;
	}

	void SetAuthCode(const std::wstring& str);

	void SetAuthUri(const std::wstring& str);

	void SetConnectButtonState(bool enabled);

	class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame>
	{
	public:
		VideoRenderer(HWND wnd, int width, int height,
			webrtc::VideoTrackInterface* track_to_render);

		virtual ~VideoRenderer();

		void Lock()
		{
			::EnterCriticalSection(&buffer_lock_);
		}

		void Unlock()
		{
			::LeaveCriticalSection(&buffer_lock_);
		}

		// VideoSinkInterface implementation
		void OnFrame(const webrtc::VideoFrame& frame) override;

		const BITMAPINFO& bmi() const
		{
			return bmi_;
		}

		const uint8_t* image() const
		{
			return image_.get();
		}

	protected:
		void SetSize(int width, int height);

		enum
		{
			SET_SIZE,
			RENDER_FRAME,
		};

		HWND wnd_;
		BITMAPINFO bmi_;
		std::unique_ptr<uint8_t[]> image_;
		CRITICAL_SECTION buffer_lock_;
		rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
	};

	// A little helper class to make sure we always to proper locking and
	// unlocking when working with VideoRenderer buffers.
	template <typename T>
	class AutoLock
	{
	public:
		explicit AutoLock(T* obj) : obj_(obj)
		{
			obj_->Lock();
		}

		~AutoLock()
		{
			obj_->Unlock();
		}

	protected:
		T* obj_;
	};

protected:
	enum ChildWindowID
	{
		EDIT_ID = 1,
		BUTTON_ID,
		LABEL1_ID,
		LABEL2_ID,
		LISTBOX_ID,
		AUTH_ID
	};

	void OnPaint();

	void OnDestroyed();

	void OnDefaultAction();

	bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

	static bool RegisterWindowClass();

	void CreateChildWindow(HWND* wnd, ChildWindowID id, const wchar_t* class_name,
		DWORD control_style, DWORD ex_style);

	void CreateChildWindows();

	void LayoutConnectUI(bool show);

	void LayoutPeerListUI(bool show);

	void HandleTabbing();

private:
	std::unique_ptr<VideoRenderer> local_renderer_;
	std::unique_ptr<VideoRenderer> remote_renderer_;
	UI ui_;
	HWND wnd_;
	DWORD ui_thread_id_;
	HWND edit1_;
	HWND edit2_;
	HWND label1_;
	HWND label2_;
	HWND button_;
	HWND listbox_;
	HWND auth_uri_;
	HWND auth_uri_label_;
	HWND auth_code_;
	HWND auth_code_label_;
	ID2D1Factory* direct2d_factory_;
	ID2D1HwndRenderTarget* render_target_;
	bool destroyed_;
	void* nested_msg_;
	MainWindowCallback* callback_;
	static ATOM wnd_class_;
	std::string server_;
	std::string port_;
	std::wstring auth_code_val_;
	std::wstring auth_uri_val_;
	bool auto_connect_;
	bool auto_call_;
	bool connect_button_state_;

	Win32DataChannelHandler* data_channel_handler_;

	int width_;
	int height_;
};

#endif  // WEBRTC_DEFAULT_MAIN_WINDOW_H_
