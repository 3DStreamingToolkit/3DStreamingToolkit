#include "stdafx.h"
#include "server_main_window.h"

#include <math.h>

#include "libyuv/convert_argb.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

using rtc::sprintfn;

namespace
{

	const char kConnecting[] = "Connecting... ";
	const char kNoVideoStreams[] = "(no video streams either way)";
	const char kNoIncomingStream[] = "(no incoming video)";

	void CalculateWindowSizeForText(HWND wnd, const wchar_t* text, size_t* width,
		size_t* height)
	{
		HDC dc = ::GetDC(wnd);
		RECT text_rc = { 0 };
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
		char text[MAX_PATH] = { 0 };
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

ServerMainWindow::ServerMainWindow(
	const char* server,
	int port,
	bool auto_connect,
	bool auto_call,
	bool has_no_UI,
	int width,
	int height) :
	MainWindow([&](HWND a, int b, int c, webrtc::VideoTrackInterface* d) { return AllocateVideoRenderer(a, b, c, d); }),
	edit1_(NULL),
	edit2_(NULL),
	label1_(NULL),
	label2_(NULL),
	button_(NULL),
	listbox_(NULL),
	auth_code_(NULL),
	auth_code_label_(NULL),
	auth_uri_(NULL),
	auth_uri_label_(NULL),
	server_(server),
	auto_connect_(auto_connect),
	auto_call_(auto_call),
	has_no_UI_(has_no_UI),
	width_(width),
	height_(height)
{
	SignalWindowMessage.connect(this, &ServerMainWindow::OnMessage);

	char buffer[10] = { 0 };
	sprintfn(buffer, sizeof(buffer), "%i", port);
	port_ = buffer;
}

ServerMainWindow::~ServerMainWindow()
{
	RTC_DCHECK(!IsWindow());
}

bool ServerMainWindow::Create()
{
	RTC_DCHECK(wnd_ == NULL);
	if (!RegisterWindowClass())
	{
		return false;
	}

	ui_thread_id_ = ::GetCurrentThreadId();
	int visibleFlag = (has_no_UI_) ? 0 : WS_VISIBLE;
	wnd_ = ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, kClassName,
		L"Server",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | visibleFlag,
		CW_USEDEFAULT, CW_USEDEFAULT, width_, height_,
		NULL, NULL, GetModuleHandle(NULL), this);

	::SendMessage(wnd_, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultFont()), TRUE);

	CreateChildWindows();
	SwitchToConnectUI();

	return wnd_ != NULL;
}

bool ServerMainWindow::Destroy()
{
	BOOL ret = FALSE;
	if (IsWindow())
	{
		ret = ::DestroyWindow(wnd_);
	}

	return ret != FALSE;
}

bool ServerMainWindow::PreTranslateMessage(MSG* msg)
{
	bool ret = false;
	if (msg->message == WM_CHAR)
	{
		if (msg->wParam == VK_TAB)
		{
			HandleTabbing();
			ret = true;
		}
		else if (msg->wParam == VK_RETURN)
		{
			OnDefaultAction();
			ret = true;
		}
		else if (msg->wParam == VK_ESCAPE)
		{
			if (!callbacks_.empty())
			{
				if (current_ui_ == STREAMING)
				{
					std::for_each(callbacks_.begin(), callbacks_.end(), [](MainWindowCallback* callback) { callback->DisconnectFromCurrentPeer(); });
				}
				else
				{
					std::for_each(callbacks_.begin(), callbacks_.end(), [](MainWindowCallback* callback) { callback->DisconnectFromServer(); });
				}
			}
		}
	}
	else if (msg->hwnd == NULL && msg->message == UI_THREAD_CALLBACK)
	{
		std::for_each(callbacks_.begin(), callbacks_.end(), [&](MainWindowCallback* callback)
		{
			callback->UIThreadCallback(static_cast<int>(msg->wParam),
				reinterpret_cast<void*>(msg->lParam));
		});
		ret = true;
	}

	return ret;
}

void ServerMainWindow::SetAuthCode(const std::wstring& str)
{
	if (IsWindow())
	{
		::SetWindowText(auth_code_, str.c_str());
	}
}

void ServerMainWindow::SetAuthUri(const std::wstring& str)
{
	if (IsWindow())
	{
		::SetWindowText(auth_uri_, str.c_str());
	}
}

void ServerMainWindow::OnPaint()
{
	PAINTSTRUCT ps;
	::BeginPaint(wnd_, &ps);

	RECT rc;
	::GetClientRect(wnd_, &rc);

	VideoRenderer* local_renderer = local_video_renderer_.get();
	if (current_ui_ == STREAMING && local_renderer)
	{
		AutoLock<VideoRenderer> local_lock(local_renderer);
		const BITMAPINFO& bmi = local_renderer->bmi();
		int height = abs(bmi.bmiHeader.biHeight);
		int width = bmi.bmiHeader.biWidth;
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

		const uint8_t* image = local_renderer->image();
		int thumb_width = bmi.bmiHeader.biWidth / 2;
		int thumb_height = abs(bmi.bmiHeader.biHeight) / 2;
		StretchDIBits(
			dc_mem,
			logical_area.x - thumb_width - 10,
			logical_area.y - thumb_height - 10,
			thumb_width,
			thumb_height,
			0,
			0,
			bmi.bmiHeader.biWidth,
			-bmi.bmiHeader.biHeight,
			image,
			&bmi,
			DIB_RGB_COLORS, SRCCOPY);

		BitBlt(ps.hdc, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0, SRCCOPY);

		// Cleanup.
		::SelectObject(dc_mem, bmp_old);
		::DeleteObject(bmp_mem);
		::DeleteDC(dc_mem);
	}
	else
	{
		HBRUSH brush = ::CreateSolidBrush(::GetSysColor(COLOR_WINDOW));
		::FillRect(ps.hdc, &rc, brush);
		::DeleteObject(brush);
	}

	::EndPaint(wnd_, &ps);
}

void ServerMainWindow::OnDefaultAction()
{
	if (callbacks_.empty())
	{
		return;
	}

	if (current_ui_ == CONNECT_TO_SERVER)
	{
		std::string server(GetWindowText(edit1_));
		std::string port_str(GetWindowText(edit2_));
		int port = port_str.length() ? atoi(port_str.c_str()) : 0;

		std::for_each(callbacks_.begin(), callbacks_.end(), [&](MainWindowCallback* callback) { callback->StartLogin(server, port); });
	}
	else if (current_ui_ == LIST_PEERS)
	{
		LRESULT sel = ::SendMessage(listbox_, LB_GETCURSEL, 0, 0);
		if (sel != LB_ERR)
		{
			LRESULT peer_id = ::SendMessage(listbox_, LB_GETITEMDATA, sel, 0);
			if (peer_id != -1 && !callbacks_.empty())
			{
				std::for_each(callbacks_.begin(), callbacks_.end(), [&](MainWindowCallback* callback) { callback->ConnectToPeer(peer_id); });
			}
		}
	}
	else
	{
		MessageBoxA(wnd_, "OK!", "Yeah", MB_OK);
	}
}

VideoRenderer* ServerMainWindow::AllocateVideoRenderer(HWND wnd, int width, int height, webrtc::VideoTrackInterface* track)
{
	return new ServerVideoRenderer(wnd, width, height, track);
}

void ServerMainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result, bool* retCode)
{
	switch (msg)
	{
	case WM_SETFOCUS:
		if (current_ui_ == CONNECT_TO_SERVER)
		{
			SetFocus(edit1_);
		}
		else if (current_ui_ == LIST_PEERS)
		{
			SetFocus(listbox_);
		}

		*retCode = true;
		break;

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

		*retCode = true;
		break;
	}
}

void ServerMainWindow::CreateChildWindow(HWND* wnd, ServerMainWindow::ChildWindowID id,
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

void ServerMainWindow::CreateChildWindows()
{
	// Create the child windows in tab order.
	CreateChildWindow(&auth_uri_label_, AUTH_ID, L"Static", ES_CENTER | ES_READONLY, 0);
	CreateChildWindow(&auth_uri_, AUTH_ID, L"Edit", ES_LEFT | ES_READONLY, 0);

	CreateChildWindow(&auth_code_label_, AUTH_ID, L"Static", ES_CENTER | ES_READONLY, 0);
	CreateChildWindow(&auth_code_, AUTH_ID, L"Edit", ES_LEFT | ES_READONLY, 0);

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

void ServerMainWindow::LayoutConnectUI(bool show)
{
	struct Windows
	{
		HWND wnd;
		const wchar_t* text;
		size_t width;
		size_t height;
	} windows[] =
	{
		{ auth_uri_label_, L"Auth Uri" },
		{ auth_uri_,L"XXXyyyYYYgggXXXyyyYYYgggXXXyyyYYYggggggXXXyyyYYYgggXXXyyyYYYgggXXXyy" },
		{ auth_code_label_, L"Auth Code" },
		{ auth_code_, L"XXXyyyYYYgggXXXyy" },
		{ label1_, L"Server" },
		{ edit1_, L"XXXyyyYYYgggXXXyyyYYYgggXXXyyyYYYggg" },
		{ label2_, L":" },
		{ edit2_, L"XyXyX" },
		{ button_, L"Connect" },
	};

	if (show)
	{
		// block scope for auth layout
		{
			const size_t kSeparator = 2;
			size_t total_width = 3 * kSeparator;

			// just auth stuff
			for (size_t i = 0; i < 4; ++i)
			{
				CalculateWindowSizeForText(windows[i].wnd, windows[i].text,
					&windows[i].width, &windows[i].height);

				total_width += windows[i].width;
			}

			RECT rc;
			::GetClientRect(wnd_, &rc);
			size_t x = (rc.right / 2) - (total_width / 2);
			size_t y = (rc.bottom / 2) - ((rc.bottom / 2) / 3);

			// just auth stuff
			for (size_t i = 0; i < 4; ++i)
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
		// end block scope for auth layout

		// block scope for connection layout
		{
			const size_t kSeparator = 5;
			size_t total_width = (ARRAYSIZE(windows) - 1 - 4 /* 4 for auth */) * kSeparator;

			// start at 4, skipping auth stuff
			for (size_t i = 4; i < ARRAYSIZE(windows); ++i)
			{
				CalculateWindowSizeForText(windows[i].wnd, windows[i].text,
					&windows[i].width, &windows[i].height);

				total_width += windows[i].width;
			}

			RECT rc;
			::GetClientRect(wnd_, &rc);
			size_t x = (rc.right / 2) - (total_width / 2);
			size_t y = (rc.bottom / 2) + ((rc.bottom / 2) / 3);

			// start at 4, skipping auth stuff
			for (size_t i = 4; i < ARRAYSIZE(windows); ++i)
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
		// end block scope for connection layout
	}
	else
	{
		for (size_t i = 0; i < ARRAYSIZE(windows); ++i)
		{
			::ShowWindow(windows[i].wnd, SW_HIDE);
		}
	}

	if (auto_connect_)
	{
		::PostMessage(button_, BM_CLICK, 0, 0);
	}
}

void ServerMainWindow::LayoutPeerListUI(const std::map<int, std::string>& peers, bool show)
{
	if (show)
	{
		::SendMessage(listbox_, LB_RESETCONTENT, 0, 0);

		AddListBoxItem(listbox_, "List of currently connected peers:", -1);
		std::map<int, std::string>::const_iterator i = peers.begin();
		for (; i != peers.end(); ++i)
			AddListBoxItem(listbox_, i->second.c_str(), i->first);

		RECT rc;
		::GetClientRect(wnd_, &rc);
		::MoveWindow(listbox_, 0, 0, rc.right, rc.bottom, TRUE);
		::ShowWindow(listbox_, SW_SHOWNA);

		if (auto_call_ && peers.begin() != peers.end())
		{
			// Get the number of items in the list
			LRESULT count = ::SendMessage(listbox_, LB_GETCOUNT, 0, 0);
			if (count != LB_ERR)
			{
				// Select the last item in the list
				LRESULT selection = ::SendMessage(listbox_, LB_SETCURSEL, count - 1, 0);
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
	else
	{
		::ShowWindow(listbox_, SW_HIDE);
		InvalidateRect(wnd_, NULL, TRUE);
	}
}

void ServerMainWindow::HandleTabbing()
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
	} while (true);

	::SetFocus(next);
}

//
// DefaultMainWindow::VideoRenderer
//

ServerMainWindow::ServerVideoRenderer::ServerVideoRenderer(HWND wnd, int width, int height,
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

ServerMainWindow::ServerVideoRenderer::~ServerVideoRenderer()
{
	rendered_track_->RemoveSink(this);
	::DeleteCriticalSection(&buffer_lock_);
}

void ServerMainWindow::ServerVideoRenderer::SetSize(int width, int height)
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

void ServerMainWindow::ServerVideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame)
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