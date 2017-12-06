/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "stdafx.h"

#include <math.h>

#include "client_main_window.h"
#include "libyuv/convert_argb.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using rtc::sprintfn;
using Microsoft::WRL::ComPtr;

namespace
{
const char kConnecting[]		= "Connecting... ";
const char kNoVideoStreams[]	= "(no video streams either way)";
const char kNoIncomingStream[]	= "(no incoming video)";

const WCHAR kFontName[]			= L"Verdana";
const FLOAT kFontSize			= 20;

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

ClientMainWindow::ClientMainWindow(
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
		direct2d_factory_(NULL),
		render_target_(NULL),
		dwrite_factory_(NULL),
		text_format_(NULL),
		brush_(NULL),
		server_(server),
		auto_connect_(auto_connect),
		auto_call_(auto_call),
		width_(width),
		height_(height),
		connect_button_state_(true)
{
	SignalWindowMessage.connect(this, &ClientMainWindow::OnMessage);

	char buffer[10] = {0};
	sprintfn(buffer, sizeof(buffer), "%i", port);
	port_ = buffer;

	wcscpy(fps_text_, L"FPS: 0");
}

ClientMainWindow::~ClientMainWindow()
{
	RTC_DCHECK(!IsWindow());
	SAFE_RELEASE(direct2d_factory_);
	SAFE_RELEASE(render_target_);
	SAFE_RELEASE(dwrite_factory_);
	SAFE_RELEASE(text_format_);
	SAFE_RELEASE(brush_);
}

bool ClientMainWindow::Create()
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

	// Creates a Direct2D factory.
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &direct2d_factory_);
	if (FAILED(hr))
	{
		return false;
	}

	// Create a DirectWrite factory.
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(dwrite_factory_),
		reinterpret_cast<IUnknown **>(&dwrite_factory_)
	);

	if (FAILED(hr))
	{
		return false;
	}

	// Create a DirectWrite text format object.
	hr = dwrite_factory_->CreateTextFormat(
		kFontName,
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		kFontSize,
		L"", // locale
		&text_format_
	);

	if (FAILED(hr))
	{
		return false;
	}

	RECT rc;
	GetClientRect(wnd_, &rc);
	D2D1_SIZE_U size = { rc.right - rc.left, rc.bottom - rc.top };

	// Create a Direct2D render target.
	hr = direct2d_factory_->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(wnd_, size),
		&render_target_
	);

	if (FAILED(hr))
	{
		return false;
	}

	// Creates a solid color brush.
	hr = render_target_->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White, 1.0f),
		&brush_
	);

	if (FAILED(hr))
	{
		return false;
	}

	::SendMessage(wnd_, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultFont()), TRUE);
	
	CreateChildWindows();
	SwitchToConnectUI();
	
	return wnd_ != NULL;
}

bool ClientMainWindow::Destroy()
{
	BOOL ret = FALSE;
	if (IsWindow())
	{
		ret = ::DestroyWindow(wnd_);
	}

	return ret != FALSE;
}

bool ClientMainWindow::PreTranslateMessage(MSG* msg)
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

				ret = true;
			}
		}
	}

	if (current_ui_ == STREAMING && !callbacks_.empty() && !ret)
	{
		SignalDataChannelMessage.emit(msg);
	}

	// UI callback
	if (msg->hwnd == NULL && msg->message == UI_THREAD_CALLBACK)
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

void ClientMainWindow::OnPaint()
{
	PAINTSTRUCT ps;
	RECT rc;
	FLOAT dpiX, dpiY;
	FLOAT scaleX, scaleY;

	::BeginPaint(wnd_, &ps);
	::GetClientRect(wnd_, &rc);
	direct2d_factory_->GetDesktopDpi(&dpiX, &dpiY);
	scaleX = 96.0f / dpiX;
	scaleY = 96.0f / dpiY;

	D2D1_RECT_F desRect =
	{
		rc.left * scaleX,
		rc.top * scaleY,
		rc.right * scaleX,
		rc.bottom * scaleY
	};

	VideoRenderer* remote_renderer = remote_video_renderer_.get();
	if (current_ui_ == STREAMING && remote_renderer)
	{
		AutoLock<VideoRenderer> remote_lock(remote_renderer);
		const BITMAPINFO& bmi = remote_renderer->bmi();
		int height = abs(bmi.bmiHeader.biHeight);
		int width = bmi.bmiHeader.biWidth;
		const uint8_t* image = remote_renderer->image();
		const int fps = ((ClientVideoRenderer*)remote_renderer)->fps();
		if (image != NULL)
		{
			// Initializes the bitmap properties.
			D2D1_BITMAP_PROPERTIES bitmapProps;
			bitmapProps.dpiX = 0;
			bitmapProps.dpiY = 0;
			bitmapProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
			bitmapProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;

			// Creates the bitmap.
			ID2D1Bitmap* bitmap = NULL;
			D2D1_SIZE_U size = { width, height };
			render_target_->CreateBitmap(size, image, width * 4, bitmapProps, &bitmap);
			
			// Starts rendering.
			render_target_->BeginDraw();

			// Renders the video frame.
			render_target_->DrawBitmap(bitmap, desRect);

			// Draws the fps info.
			wsprintf(fps_text_, L"FPS: %d", fps);
			render_target_->DrawText(
				fps_text_,
				ARRAYSIZE(fps_text_) - 1,
				text_format_,
				desRect,
				brush_
			);

			// Ends rendering.
			render_target_->EndDraw();

			// Releases the bitmap.
			SAFE_RELEASE(bitmap);
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

	::EndPaint(wnd_, &ps);
}

void ClientMainWindow::OnDefaultAction()
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


VideoRenderer* ClientMainWindow::AllocateVideoRenderer(HWND wnd, int width, int height, webrtc::VideoTrackInterface* track)
{
	return new ClientVideoRenderer(wnd, width, height, track);
}

void ClientMainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result, bool* retCode)
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

		case WM_SIZE:
			if (current_ui_ == STREAMING && render_target_)
			{
				RECT rc;
				GetClientRect(wnd_, &rc);
				D2D1_SIZE_U size = { rc.right - rc.left, rc.bottom - rc.top };
				render_target_->Resize(size);
				SignalClientWindowMessage.emit(msg, wp, lp);
			}
			
			break;

		case WM_CTLCOLORSTATIC:
			*result = reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
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
	}
}

void ClientMainWindow::CreateChildWindow(HWND* wnd, ClientMainWindow::ChildWindowID id,
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

void ClientMainWindow::CreateChildWindows()
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

void ClientMainWindow::LayoutConnectUI(bool show)
{
	struct Windows
	{
		HWND wnd;
		const wchar_t* text;
		size_t width;
		size_t height;
	} windows[] =
	{
		{ auth_uri_label_, L"Auth Uri"},
		{ auth_uri_,L"XXXyyyYYYgggXXXyyyYYYgggXXXyyyYYYggggggXXXyyyYYYgggXXXyyyYYYgggXXXyy" },
		{ auth_code_label_, L"Auth Code" },
		{ auth_code_, L"XXXyyyYYYgggXXXyy" },
		{ label1_, L"Server" },
		{ edit1_, L"XXXyyyYYYgggXXXyyyYYYgggXXXyyyYYYggggggXXXyyyYYYgggXXXyyyYYYgggXXXyy" },
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

				// if the window is either a) not the connect button
				// or b) the connect button, and connect_button_state_
				// is true
				if (windows[i].wnd != button_ ||
					(button_ != NULL && windows[i].wnd == button_ && connect_button_state_))
				{
					::ShowWindow(windows[i].wnd, SW_SHOWNA);
				}
				else if (button_ != NULL && windows[i].wnd == button_ && !connect_button_state_)
				{
					::ShowWindow(windows[i].wnd, SW_HIDE);
				}
			}
		}
		// end block scope for connection layout
	} 
	else
	{
		// hide all windows
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

void ClientMainWindow::LayoutPeerListUI(const std::map<int, std::string>& peers, bool show)
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

void ClientMainWindow::HandleTabbing()
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

void ClientMainWindow::SetAuthCode(const std::wstring& str)
{
	::SetWindowText(auth_code_, str.c_str());
}

void ClientMainWindow::SetAuthUri(const std::wstring& str)
{
	::SetWindowText(auth_uri_, str.c_str());
}

void ClientMainWindow::SetConnectButtonState(bool enabled)
{
	connect_button_state_ = enabled;
}

//
// ClientMainWindow::VideoRenderer
//

ClientMainWindow::ClientVideoRenderer::ClientVideoRenderer(HWND wnd, int width, int height,
    webrtc::VideoTrackInterface* track_to_render) :
		wnd_(wnd),
		rendered_track_(track_to_render),
		time_tick_(0),
		frame_counter_(0)
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

ClientMainWindow::ClientVideoRenderer::~ClientVideoRenderer()
{
	rendered_track_->RemoveSink(this);
	::DeleteCriticalSection(&buffer_lock_);
}

void ClientMainWindow::ClientVideoRenderer::SetSize(int width, int height)
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

void ClientMainWindow::ClientVideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame)
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

	// Updates FPS.
	frame_counter_++;
	if (GetTickCount64() - time_tick_ >= 1000)
	{
		fps_ = frame_counter_;
		frame_counter_ = 0;
		time_tick_ = GetTickCount64();
	}
}
