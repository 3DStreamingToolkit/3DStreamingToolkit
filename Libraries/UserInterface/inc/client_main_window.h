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
#include <d2d1.h>

#include "main_window.h"

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/win32.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"

class ClientMainWindow : public MainWindow, public sigslot::has_slots<>
{
public:
	enum WindowMessages 
	{
		UI_THREAD_CALLBACK = WM_APP + 1,
	};

	ClientMainWindow(
		const char* server,
		int port,
		bool auto_connect,
		bool auto_call,
		bool has_no_UI = false,
		int width = CW_USEDEFAULT,
		int height = CW_USEDEFAULT);

	~ClientMainWindow();

	sigslot::signal3<UINT, WPARAM, LPARAM> SignalClientWindowMessage;

	bool PreTranslateMessage(MSG* msg);

	virtual bool Create() override;

	virtual bool Destroy() override;

	virtual void SetAuthCode(const std::wstring& str) override;

	virtual void SetAuthUri(const std::wstring& str) override;

	virtual void LayoutConnectUI(bool visible) override;

	virtual void LayoutPeerListUI(const std::map<int, std::string>& peers, bool visible) override;

	virtual void OnDefaultAction() override;

	virtual void OnPaint() override;
	
	void SetConnectButtonState(bool enabled);

	class ClientVideoRenderer : public VideoRenderer
	{
	public:
		ClientVideoRenderer(HWND wnd, int width, int height,
			webrtc::VideoTrackInterface* track_to_render);

		virtual ~ClientVideoRenderer();

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

		const int fps() const
		{
			return fps_;
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
		ULONGLONG time_tick_;
		int frame_counter_;
		int fps_;
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

	void OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result, bool* retCode);

	VideoRenderer* AllocateVideoRenderer(HWND wnd, int width, int height, webrtc::VideoTrackInterface* track);

	void CreateChildWindow(HWND* wnd, ChildWindowID id, const wchar_t* class_name,
		DWORD control_style, DWORD ex_style);

	void CreateChildWindows();

	void HandleTabbing();

private:
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
	IDWriteFactory* dwrite_factory_;
	IDWriteTextFormat* text_format_;
	ID2D1SolidColorBrush* brush_;
	std::string server_;
	std::string port_;
	bool auto_connect_;
	bool auto_call_;
	bool connect_button_state_;
	WCHAR fps_text_[64];

	int width_;
	int height_;
};

#endif  // WEBRTC_DEFAULT_MAIN_WINDOW_H_
