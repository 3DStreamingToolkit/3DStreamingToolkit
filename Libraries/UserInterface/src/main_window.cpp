#include "stdafx.h"
#include "main_window.h"

ATOM MainWindow::wnd_class_ = 0;
const wchar_t MainWindow::kClassName[] = L"WebRTC_MainWindow";
const std::map<int, std::string> MainWindow::kEmptyPeersMap = std::map<int, std::string>();

MainWindow::MainWindow(VideoRendererAllocator videoRendererAllocator) :
	video_renderer_alloc_(videoRendererAllocator),
	wnd_(NULL),
	destroyed_(false),
	nested_msg_(NULL),
	current_ui_(UI::CONNECT_TO_SERVER)
{
}

MainWindow::~MainWindow()
{
	RTC_DCHECK(!IsWindow());
}

void MainWindow::RegisterObserver(MainWindowCallback* callback)
{
	callbacks_.push_back(callback);
}

bool MainWindow::IsWindow()
{
	return wnd_ && ::IsWindow(wnd_) != FALSE;
}

void MainWindow::MessageBox(const char* caption, const char* text, bool is_error)
{
	DWORD flags = MB_OK;
	if (is_error)
	{
		flags |= MB_ICONERROR;
	}

	::MessageBoxA(wnd_, text, caption, flags);
}

MainWindow::UI MainWindow::current_ui()
{
	return current_ui_;
}

HWND MainWindow::handle()
{
	return wnd_;
}

void MainWindow::SwitchToConnectUI()
{
	RTC_DCHECK(IsWindow());
	LayoutPeerListUI(kEmptyPeersMap, false);
	current_ui_ = CONNECT_TO_SERVER;
	LayoutConnectUI(true);
}

void MainWindow::SwitchToPeerList(const std::map<int, std::string>& peers)
{
	RTC_DCHECK(IsWindow());
	LayoutConnectUI(false);
	current_ui_ = LIST_PEERS;
	LayoutPeerListUI(peers, true);
}

void MainWindow::SwitchToStreamingUI()
{
	LayoutConnectUI(false);
	LayoutPeerListUI(kEmptyPeersMap, false);
	current_ui_ = STREAMING;
	InvalidateRect(wnd_, NULL, true);
}

void MainWindow::StartLocalRenderer(webrtc::VideoTrackInterface* local_video)
{
	local_video_renderer_.reset(video_renderer_alloc_(wnd_, 1, 1, local_video));
}

void MainWindow::StopLocalRenderer()
{
	local_video_renderer_.reset();
}

void MainWindow::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video)
{
	remote_video_renderer_.reset(video_renderer_alloc_(wnd_, 1, 1, remote_video));
}

void MainWindow::StopRemoteRenderer()
{
	remote_video_renderer_.reset();
}

void MainWindow::QueueUIThreadCallback(int msg_id, void* data)
{
	::PostThreadMessage(ui_thread_id_, UI_THREAD_CALLBACK,
		static_cast<WPARAM>(msg_id), reinterpret_cast<LPARAM>(data));
}

void MainWindow::OnDestroyed()
{
	PostQuitMessage(0);
}

bool MainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result)
{
	bool retCode = false;

	switch (msg)
	{
	case WM_ERASEBKGND:
		*result = TRUE;
		retCode = true;
		break;
	case WM_PAINT:
		OnPaint();
		retCode = true;
		break;
	case WM_SIZE:
		if (current_ui_ == CONNECT_TO_SERVER)
		{
			LayoutConnectUI(true);
		}
		else if (current_ui_ == LIST_PEERS)
		{
			LayoutPeerListUI(kEmptyPeersMap, true);
		}
		break;
	case WM_CTLCOLORSTATIC:
		*result = reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
		retCode = true;
		break;
	case WM_CLOSE:
		if (!callbacks_.empty())
		{
			std::for_each(callbacks_.begin(), callbacks_.end(), [](MainWindowCallback* callback) { callback->Close(); });
		}
		break;
	}

	SignalWindowMessage.emit(msg, wp, lp, result, &retCode);

	return retCode;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	MainWindow* me = reinterpret_cast<MainWindow*>(
		::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (!me && WM_CREATE == msg)
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lp);
		me = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
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

bool MainWindow::RegisterWindowClass()
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