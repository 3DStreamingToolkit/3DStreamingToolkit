#pragma once

#include <map>
#include <memory>
#include <string>

#include "main_window.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/win32.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"

class ServerMainWindow : public MainWindow, public sigslot::has_slots<>
{
public:
	enum WindowMessages
	{
		UI_THREAD_CALLBACK = WM_APP + 1,
	};

	ServerMainWindow(
		const char* server,
		int port,
		bool auto_connect,
		bool auto_call,
		bool has_no_UI = false,
		int width = CW_USEDEFAULT,
		int height = CW_USEDEFAULT);

	~ServerMainWindow();

	bool PreTranslateMessage(MSG* msg);

	virtual bool Create() override;

	virtual bool Destroy() override;

	virtual void SetAuthCode(const std::wstring& str) override;

	virtual void SetAuthUri(const std::wstring& str) override;

	virtual void LayoutConnectUI(bool visible) override;

	virtual void LayoutPeerListUI(const std::map<int, std::string>& peers, bool visible) override;

	virtual void OnDefaultAction() override;
	
	virtual void OnPaint() override;
	

	class ServerVideoRenderer : public VideoRenderer
	{
	public:
		ServerVideoRenderer(HWND wnd, int width, int height,
			webrtc::VideoTrackInterface* track_to_render);

		virtual ~ServerVideoRenderer();

		virtual void Lock() override
		{
			::EnterCriticalSection(&buffer_lock_);
		}

		virtual void Unlock() override
		{
			::LeaveCriticalSection(&buffer_lock_);
		}

		// VideoSinkInterface implementation
		void OnFrame(const webrtc::VideoFrame& frame) override;

		virtual const BITMAPINFO& bmi() const override
		{
			return bmi_;
		}

		virtual const uint8_t* image() const override
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

	VideoRenderer* AllocateVideoRenderer(HWND wnd, int width, int height, webrtc::VideoTrackInterface* track);

	void OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result, bool* retCode);

	void CreateChildWindow(HWND* wnd, ChildWindowID id, const wchar_t* class_name,
		DWORD control_style, DWORD ex_style);

	void CreateChildWindows();

	void HandleTabbing();

private:
	bool has_no_UI_;
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
	std::string server_;
	std::string port_;
	bool auto_connect_;
	bool auto_call_;
	int width_;
	int height_;
};