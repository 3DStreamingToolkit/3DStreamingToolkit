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

#include <math.h>

#include "default_main_window.h"
#include "win32_data_channel_handler.h"
#include "defaults.h"
#include "libyuv/convert_argb.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using rtc::sprintfn;
using Microsoft::WRL::ComPtr;

ATOM DefaultMainWindow::wnd_class_ = 0;
const wchar_t DefaultMainWindow::kClassName[] = L"WebRTC_MainWindow";

namespace
{
const char kConnecting[] = "Connecting... ";
const char kNoVideoStreams[] = "(no video streams either way)";
const char kNoIncomingStream[] = "(no incoming video)";

void CalculateWindowSizeForText(HWND wnd, const wchar_t* text, size_t* width,
	size_t* height)
{
	HDC dc = ::GetDC(wnd);
	RECT text_rc = {0};
	::DrawText(dc, text, -1, &text_rc, DT_CALCRECT | DT_SINGLELINE);
	::ReleaseDC(wnd, dc);
	RECT client, window;
	::GetClientRect(wnd, &client);
	::GetWindowRect(wnd, &window);

	*width = text_rc.right - text_rc.left;
	*width += (window.right - window.left) - (client.right - client.left);
	*height = text_rc.bottom - text_rc.top;
	*height += (window.bottom - window.top) - (client.bottom - client.top);
}

HFONT GetDefaultFont()
{
	static HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	return font;
}

std::string GetWindowText(HWND wnd)
{
	char text[MAX_PATH] = {0};
	::GetWindowTextA(wnd, &text[0], ARRAYSIZE(text));
	return text;
}

void AddListBoxItem(HWND listbox, const std::string& str, LPARAM item_data)
{
	LRESULT index = ::SendMessageA(listbox, LB_ADDSTRING, 0,
		reinterpret_cast<LPARAM>(str.c_str()));

	::SendMessageA(listbox, LB_SETITEMDATA, index, item_data);
}

}  // namespace

DefaultMainWindow::DefaultMainWindow(
	const char* server,
	int port,
	bool auto_connect,
	bool auto_call,
	bool has_no_UI,
	int width,
	int height) : 
		ui_(CONNECT_TO_SERVER),
		wnd_(NULL),
		edit1_(NULL),
		edit2_(NULL),
		label1_(NULL),
		label2_(NULL),
		button_(NULL),
		listbox_(NULL),
		destroyed_(false),
		callback_(NULL),
		data_channel_handler_(NULL),
		nested_msg_(NULL),
		server_(server),
		auto_connect_(auto_connect),
		auto_call_(auto_call),
		width_(width),
		height_(height)
{
	char buffer[10] = {0};
	sprintfn(buffer, sizeof(buffer), "%i", port);
	port_ = buffer;
}

DefaultMainWindow::~DefaultMainWindow()
{
	RTC_DCHECK(!IsWindow());
}

bool DefaultMainWindow::Create()
{
	RTC_DCHECK(wnd_ == NULL);
	if (!RegisterWindowClass())
	{
		return false;
	}

	ui_thread_id_ = ::GetCurrentThreadId();
	wnd_ = ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, kClassName,
		L"Client",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, width_, height_,
		NULL, NULL, GetModuleHandle(NULL), this);

	::SendMessage(wnd_, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultFont()), TRUE);
	
	CreateChildWindows();
	SwitchToConnectUI();
	
	return wnd_ != NULL;
}

bool DefaultMainWindow::Destroy()
{
	BOOL ret = FALSE;
	if (IsWindow())
	{
		ret = ::DestroyWindow(wnd_);
	}

	delete data_channel_handler_;
	data_channel_handler_ = NULL;

	return ret != FALSE;
}

void DefaultMainWindow::RegisterObserver(MainWindowCallback* callback) 
{
	callback_ = callback;

	if (!data_channel_handler_)
	{
		data_channel_handler_ = new Win32DataChannelHandler(callback);
	}
}

bool DefaultMainWindow::IsWindow()
{
	return wnd_ && ::IsWindow(wnd_) != FALSE;
}

bool DefaultMainWindow::PreTranslateMessage(MSG* msg)
{
	bool ret = false;
	WPARAM wParam = msg->wParam;

	// Keyboard
	if (msg->message == WM_CHAR || msg->message == WM_KEYDOWN)
	{
		if (wParam == VK_TAB)
		{
			HandleTabbing();
			ret = true;
		} 
		else if (wParam == VK_RETURN)
		{
			OnDefaultAction();
			ret = true;
		}
		else if (wParam == VK_ESCAPE)
		{
			if (callback_)
			{
				if (ui_ == STREAMING)
				{
					callback_->DisconnectFromCurrentPeer();
				} 
				else
				{
					callback_->DisconnectFromServer();
				}

				ret = true;
			}
		}
	}

	if (ui_ == STREAMING && callback_ && !ret)
	{
		data_channel_handler_->ProcessMessage(msg);
	}

	// UI callback
	if (msg->hwnd == NULL && msg->message == UI_THREAD_CALLBACK)
	{
		callback_->UIThreadCallback(static_cast<int>(msg->wParam), 
			reinterpret_cast<void*>(msg->lParam));

		ret = true;
	}

	return ret;
}

void DefaultMainWindow::SwitchToConnectUI()
{
	RTC_DCHECK(IsWindow());
	LayoutPeerListUI(false);
	ui_ = CONNECT_TO_SERVER;
	LayoutConnectUI(true);
	::SetFocus(edit1_);

	if (auto_connect_)
	{
		::PostMessage(button_, BM_CLICK, 0, 0);
	}
}

void DefaultMainWindow::SwitchToPeerList(const map<int, string>& peers)
{
	LayoutConnectUI(false);

	::SendMessage(listbox_, LB_RESETCONTENT, 0, 0);

	AddListBoxItem(listbox_, "List of currently connected peers:", -1);
	map<int, string>::const_iterator i = peers.begin();
	for (; i != peers.end(); ++i)
	AddListBoxItem(listbox_, i->second.c_str(), i->first);

	ui_ = LIST_PEERS;
	LayoutPeerListUI(true);
	::SetFocus(listbox_);

	if (auto_call_ && peers.begin() != peers.end())
	{
		// Get the number of items in the list
		LRESULT count = ::SendMessage(listbox_, LB_GETCOUNT, 0, 0);
		if (count != LB_ERR)
		{
			// Select the last item in the list
			LRESULT selection = ::SendMessage(listbox_, LB_SETCURSEL , count - 1, 0);
			if (selection != LB_ERR)
			{
				::PostMessage(
					wnd_,
					WM_COMMAND,
					MAKEWPARAM(GetDlgCtrlID(listbox_), LBN_DBLCLK),
					reinterpret_cast<LPARAM>(listbox_));
			}
		}
	}
}

void DefaultMainWindow::SwitchToStreamingUI()
{
	LayoutConnectUI(false);
	LayoutPeerListUI(false);
	ui_ = STREAMING;
}

void DefaultMainWindow::MessageBox(const char* caption, const char* text, bool is_error) 
{
	DWORD flags = MB_OK;
	if (is_error)
	{
		flags |= MB_ICONERROR;
	}

	::MessageBoxA(handle(), text, caption, flags);
}


void DefaultMainWindow::StartLocalRenderer(webrtc::VideoTrackInterface* local_video)
{
}

void DefaultMainWindow::StopLocalRenderer()
{
}

void DefaultMainWindow::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video)
{
	remote_renderer_.reset(new VideoRenderer(handle(), 1, 1, remote_video));
}

void DefaultMainWindow::StopRemoteRenderer()
{
	remote_renderer_.reset();
}

void DefaultMainWindow::QueueUIThreadCallback(int msg_id, void* data)
{
	::PostThreadMessage(ui_thread_id_, UI_THREAD_CALLBACK,
		static_cast<WPARAM>(msg_id), reinterpret_cast<LPARAM>(data));
}

void DefaultMainWindow::OnPaint()
{
	PAINTSTRUCT ps;
	::BeginPaint(handle(), &ps);

	RECT rc;
	::GetClientRect(handle(), &rc);

	VideoRenderer* remote_renderer = remote_renderer_.get();
	if (ui_ == STREAMING && remote_renderer)
	{
		AutoLock<VideoRenderer> remote_lock(remote_renderer);
		const BITMAPINFO& bmi = remote_renderer->bmi();
		int height = abs(bmi.bmiHeader.biHeight);
		int width = bmi.bmiHeader.biWidth;
		const uint8_t* image = remote_renderer->image();
		if (image != NULL)
		{
			HDC dc_mem = ::CreateCompatibleDC(ps.hdc);
			::SetStretchBltMode(dc_mem, HALFTONE);

			// Set the map mode so that the ratio will be maintained for us.
			HDC all_dc[] = 
			{
				ps.hdc,
				dc_mem
			};

			for (int i = 0; i < arraysize(all_dc); ++i)
			{
				SetMapMode(all_dc[i], MM_ISOTROPIC);
				SetWindowExtEx(all_dc[i], width, height, NULL);
				SetViewportExtEx(all_dc[i], rc.right, rc.bottom, NULL);
			}

			HBITMAP bmp_mem = ::CreateCompatibleBitmap(ps.hdc, rc.right, rc.bottom);
			HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);

			POINT logical_area =
			{
				rc.right,
				rc.bottom
			};

			DPtoLP(ps.hdc, &logical_area, 1);

			HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
			RECT logical_rect = 
			{
				0, 0, logical_area.x, logical_area.y
			};

			::FillRect(dc_mem, &logical_rect, brush);
			::DeleteObject(brush);

			int x = (logical_area.x / 2) - (width / 2);
			int y = (logical_area.y / 2) - (height / 2);

			StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image,
				&bmi, DIB_RGB_COLORS, SRCCOPY);

			BitBlt(ps.hdc, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);

			// Cleanup.
			::SelectObject(dc_mem, bmp_old);
			::DeleteObject(bmp_mem);
			::DeleteDC(dc_mem);
		}
		else
		{
			// We're still waiting for the video stream to be initialized.
			HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
			::FillRect(ps.hdc, &rc, brush);
			::DeleteObject(brush);

			HGDIOBJ old_font = ::SelectObject(ps.hdc, GetDefaultFont());
			::SetTextColor(ps.hdc, RGB(0xff, 0xff, 0xff));
			::SetBkMode(ps.hdc, TRANSPARENT);

			std::string text(kConnecting);
			text += kNoIncomingStream;

			::DrawTextA(ps.hdc, text.c_str(), -1, &rc,
				DT_SINGLELINE | DT_CENTER | DT_VCENTER);

			::SelectObject(ps.hdc, old_font);
		}
	}
	else
	{
		HBRUSH brush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
		::FillRect(ps.hdc, &rc, brush);
		::DeleteObject(brush);
	}

	::EndPaint(handle(), &ps);
}

void DefaultMainWindow::OnDestroyed()
{
	PostQuitMessage(0);
}

void DefaultMainWindow::OnDefaultAction()
{
	if (!callback_)
	{
		return;
	}

	if (ui_ == CONNECT_TO_SERVER)
	{
		std::string server(GetWindowText(edit1_));
		std::string port_str(GetWindowText(edit2_));
		int port = port_str.length() ? atoi(port_str.c_str()) : 0;
		callback_->StartLogin(server, port);
	}
	else if (ui_ == LIST_PEERS)
	{
		LRESULT sel = ::SendMessage(listbox_, LB_GETCURSEL, 0, 0);
		if (sel != LB_ERR)
		{
			LRESULT peer_id = ::SendMessage(listbox_, LB_GETITEMDATA, sel, 0);
			if (peer_id != -1 && callback_)
			{
				callback_->ConnectToPeer(peer_id);
			}
		}
	}
	else
	{
		MessageBoxA(wnd_, "OK!", "Yeah", MB_OK);
	}
}

bool DefaultMainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result)
{
	switch (msg)
	{
		case WM_ERASEBKGND:
			*result = TRUE;
			return true;

		case WM_PAINT:
			OnPaint();
			return true;

		case WM_SETFOCUS:
			if (ui_ == CONNECT_TO_SERVER) 
			{
				SetFocus(edit1_);
			}
			else if (ui_ == LIST_PEERS)
			{
				SetFocus(listbox_);
			}

			return true;

		case WM_SIZE:
			if (ui_ == CONNECT_TO_SERVER)
			{
				LayoutConnectUI(true);
			}
			else if (ui_ == LIST_PEERS)
			{
				LayoutPeerListUI(true);
			}
			
			break;

		case WM_CTLCOLORSTATIC:
			*result = reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
			return true;

		case WM_COMMAND:
			if (button_ == reinterpret_cast<HWND>(lp))
			{
				if (BN_CLICKED == HIWORD(wp))
				{
					OnDefaultAction();
				}
			}
			else if (listbox_ == reinterpret_cast<HWND>(lp))
			{
				if (LBN_DBLCLK == HIWORD(wp))
				{
					OnDefaultAction();
				}
			}

			return true;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Keyboard::ProcessMessage(msg, wp, lp);
			break;

		case WM_INPUT:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_MOUSEHOVER:
			Mouse::ProcessMessage(msg, wp, lp);
			break;

		case WM_CLOSE:
			if (callback_)
			{
				callback_->Close();
				callback_ = NULL;
			}

			break;
	}

	return false;
}

// static
LRESULT CALLBACK DefaultMainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	DefaultMainWindow* me = reinterpret_cast<DefaultMainWindow*>(
		::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (!me && WM_CREATE == msg)
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
		me = reinterpret_cast<DefaultMainWindow*>(cs->lpCreateParams);
		me->wnd_ = hwnd;
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(me));
	}

	LRESULT result = 0;
	if (me)
	{
		void* prev_nested_msg = me->nested_msg_;
		me->nested_msg_ = &msg;

		bool handled = me->OnMessage(msg, wp, lp, &result);
		if (WM_NCDESTROY == msg)
		{
			me->destroyed_ = true;
		}
		else if (!handled) 
		{
			result = ::DefWindowProc(hwnd, msg, wp, lp);
		}

		if (me->destroyed_ && prev_nested_msg == NULL)
		{
			me->OnDestroyed();
			me->wnd_ = NULL;
			me->destroyed_ = false;
		}

		me->nested_msg_ = prev_nested_msg;
	}
	else
	{
		result = ::DefWindowProc(hwnd, msg, wp, lp);
	}

	return result;
}

// static
bool DefaultMainWindow::RegisterWindowClass()
{
	if (wnd_class_)
	{
		return true;
	}

	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	wcex.style = CS_DBLCLKS;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wcex.lpfnWndProc = &WndProc;
	wcex.lpszClassName = kClassName;
	wnd_class_ = ::RegisterClassEx(&wcex);
	RTC_DCHECK(wnd_class_ != 0);
	return wnd_class_ != 0;
}

void DefaultMainWindow::CreateChildWindow(HWND* wnd, DefaultMainWindow::ChildWindowID id,
	const wchar_t* class_name, DWORD control_style, DWORD ex_style)
{
	if (::IsWindow(*wnd))
	{
		return;
	}

	// Child windows are invisible at first, and shown after being resized.
	DWORD style = WS_CHILD | control_style;
	*wnd = ::CreateWindowEx(ex_style, class_name, L"", style, 100, 100, 100, 100, 
		wnd_, reinterpret_cast<HMENU>(id), GetModuleHandle(NULL), NULL);

	RTC_DCHECK(::IsWindow(*wnd) != FALSE);
	::SendMessage(*wnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultFont()), TRUE);
}

void DefaultMainWindow::CreateChildWindows()
{
	// Create the child windows in tab order.
	CreateChildWindow(&label1_, LABEL1_ID, L"Static", ES_CENTER | ES_READONLY, 0);
	CreateChildWindow(&edit1_, EDIT_ID, L"Edit", ES_LEFT | ES_NOHIDESEL | WS_TABSTOP,
		WS_EX_CLIENTEDGE);

	CreateChildWindow(&label2_, LABEL2_ID, L"Static", ES_CENTER | ES_READONLY, 0);
	CreateChildWindow(&edit2_, EDIT_ID, L"Edit", ES_LEFT | ES_NOHIDESEL | WS_TABSTOP,
		WS_EX_CLIENTEDGE);

	CreateChildWindow(&button_, BUTTON_ID, L"Button", BS_CENTER | WS_TABSTOP, 0);

	CreateChildWindow(&listbox_, LISTBOX_ID, L"ListBox", 
		LBS_HASSTRINGS | LBS_NOTIFY, WS_EX_CLIENTEDGE);

	::SetWindowTextA(edit1_, server_.c_str());
	::SetWindowTextA(edit2_, port_.c_str());
}

void DefaultMainWindow::LayoutConnectUI(bool show)
{
	struct Windows
	{
		HWND wnd;
		const wchar_t* text;
		size_t width;
		size_t height;
	} windows[] =
	{
		{ label1_, L"Server" },
		{ edit1_, L"XXXyyyYYYgggXXXyyyYYYgggXXXyyyYYYggg" },
		{ label2_, L":" },
		{ edit2_, L"XyXyX" },
		{ button_, L"Connect" },
	};

	if (show)
	{
		const size_t kSeparator = 5;
		size_t total_width = (ARRAYSIZE(windows) - 1) * kSeparator;

		for (size_t i = 0; i < ARRAYSIZE(windows); ++i)
		{
			CalculateWindowSizeForText(windows[i].wnd, windows[i].text, 
				&windows[i].width, &windows[i].height);

			total_width += windows[i].width;
		}

		RECT rc;
		::GetClientRect(wnd_, &rc);
		size_t x = (rc.right / 2) - (total_width / 2);
		size_t y = rc.bottom / 2;
		for (size_t i = 0; i < ARRAYSIZE(windows); ++i)
		{
			size_t top = y - (windows[i].height / 2);
			::MoveWindow(windows[i].wnd, static_cast<int>(x), static_cast<int>(top), 
				static_cast<int>(windows[i].width), static_cast<int>(windows[i].height), TRUE);

			x += kSeparator + windows[i].width;
			if (windows[i].text[0] != 'X')
			{
				::SetWindowText(windows[i].wnd, windows[i].text);
			}

			::ShowWindow(windows[i].wnd, SW_SHOWNA);
		}
	} 
	else
	{
		for (size_t i = 0; i < ARRAYSIZE(windows); ++i)
		{
			::ShowWindow(windows[i].wnd, SW_HIDE);
		}
	}
}

void DefaultMainWindow::LayoutPeerListUI(bool show)
{
	if (show)
	{
		RECT rc;
		::GetClientRect(wnd_, &rc);
		::MoveWindow(listbox_, 0, 0, rc.right, rc.bottom, TRUE);
		::ShowWindow(listbox_, SW_SHOWNA);
	}
	else
	{
		::ShowWindow(listbox_, SW_HIDE);
		InvalidateRect(wnd_, NULL, TRUE);
	}
}

void DefaultMainWindow::HandleTabbing()
{
	bool shift = ((::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
	UINT next_cmd = shift ? GW_HWNDPREV : GW_HWNDNEXT;
	UINT loop_around_cmd = shift ? GW_HWNDLAST : GW_HWNDFIRST;
	HWND focus = GetFocus(), next;

	do
	{
		next = ::GetWindow(focus, next_cmd);
		if (IsWindowVisible(next) && (GetWindowLong(next, GWL_STYLE) & WS_TABSTOP))
		{
			break;
		}

		if (!next)
		{
			next = ::GetWindow(focus, loop_around_cmd);
			if (IsWindowVisible(next) && (GetWindowLong(next, GWL_STYLE) & WS_TABSTOP))
			{
				break;
			}
		}

		focus = next;
	}
	while (true);

	::SetFocus(next);
}

//
// DefaultMainWindow::VideoRenderer
//

DefaultMainWindow::VideoRenderer::VideoRenderer(HWND wnd, int width, int height,
    webrtc::VideoTrackInterface* track_to_render) :
		wnd_(wnd),
		rendered_track_(track_to_render)
{
	::InitializeCriticalSection(&buffer_lock_);
	ZeroMemory(&bmi_, sizeof(bmi_));
	bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi_.bmiHeader.biPlanes = 1;
	bmi_.bmiHeader.biBitCount = 32;
	bmi_.bmiHeader.biCompression = BI_RGB;
	bmi_.bmiHeader.biWidth = width;
	bmi_.bmiHeader.biHeight = -height;
	bmi_.bmiHeader.biSizeImage = width * height * (bmi_.bmiHeader.biBitCount >> 3);
	rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

DefaultMainWindow::VideoRenderer::~VideoRenderer()
{
	rendered_track_->RemoveSink(this);
	::DeleteCriticalSection(&buffer_lock_);
}

void DefaultMainWindow::VideoRenderer::SetSize(int width, int height)
{
	AutoLock<VideoRenderer> lock(this);

	if (width == bmi_.bmiHeader.biWidth && height == bmi_.bmiHeader.biHeight)
	{
		return;
	}

	bmi_.bmiHeader.biWidth = width;
	bmi_.bmiHeader.biHeight = -height;
	bmi_.bmiHeader.biSizeImage = width * height * (bmi_.bmiHeader.biBitCount >> 3);
	image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
}

void DefaultMainWindow::VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) 
{
	AutoLock<VideoRenderer> lock(this);

	rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
		video_frame.video_frame_buffer());

	if (video_frame.rotation() != webrtc::kVideoRotation_0) 
	{
		buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
	}

	SetSize(buffer->width(), buffer->height());

	RTC_DCHECK(image_.get() != NULL);
	libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
		buffer->DataU(), buffer->StrideU(),
		buffer->DataV(), buffer->StrideV(),
		image_.get(),
		bmi_.bmiHeader.biWidth *
		bmi_.bmiHeader.biBitCount / 8,
		buffer->width(), buffer->height());

	InvalidateRect(wnd_, NULL, TRUE);
}
