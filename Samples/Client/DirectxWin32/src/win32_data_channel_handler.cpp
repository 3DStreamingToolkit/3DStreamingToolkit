#include "pch.h"

#include <algorithm>

#include "win32_data_channel_handler.h"
#include "minwindef.h"

#include "webrtc/base/json.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

static UINT s_wm_wakeup_id;

Win32DataChannelHandler::Win32DataChannelHandler(DataChannelCallback* data_channel_callback) :
	DataChannelHandler(data_channel_callback),
	stereo_mode_(-1),
	width_(0),
	height_(0),
	keyboardTick(0),
	mouseTick(0),
	zoom_(DEFAULT_ZOOM),
	distance_(DEFAULT_DISTANCE)
{
	s_wm_wakeup_id = RegisterWindowMessage(L"WM_WAKEUP");
	mouse_ = std::make_unique<Mouse>();
}

Win32DataChannelHandler::~Win32DataChannelHandler()
{
}

void Win32DataChannelHandler::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Avoids blocking win32socketserver
	if (message == s_wm_wakeup_id)
	{
		return;
	}

	Vector3 move = Vector3::Zero;
	float scale = distance_;
	bool sendMessage = false;
	bool sendMouseEvent = false;

	// Requests mono stream.
	if (stereo_mode_ == -1 && RequestStereoStream(false))
	{
		stereo_mode_ = 0;
	}

	switch (message)
	{
	case WM_SIZE:
		ball_camera_.SetWindow(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_CHAR:
	case WM_KEYDOWN:
	{
		Json::StyledWriter writer;
		Json::Value jmessage;
		jmessage["message"] = message;
		jmessage["wParam"] = wParam;
		SendKeyboardInput(writer.write(jmessage));
	}

	switch (wParam)
	{
	case VK_SHIFT:
		scale *= CAMERA_MOVEMENT_SCALE;
		break;

	case VK_UP:
		move.y += scale;
		break;

	case VK_DOWN:
		move.y -= scale;
		break;

	case VK_RIGHT:
	case 'D':
		move.x += scale;
		break;

	case VK_LEFT:
	case 'A':
		move.x -= scale;
		break;

	case 'W':
		move.z += scale;
		break;

	case 'S':
		move.z -= scale;
		break;

	case VK_HOME:
		sendMessage = true;
		ResetCamera();
		break;
	}

	if (move != Vector3::Zero)
	{
		Matrix im;
		view_.Invert(im);
		move = Vector3::TransformNormal(move, im);
		camera_focus_ += move;
		if (++keyboardTick == KEYBOARD_DELAY)
		{
			sendMessage = true;
			keyboardTick = 0;
		}
	}

	break;

	case WM_MOUSEHOVER:
	case WM_MOUSELEAVE:
	case WM_MOUSEHWHEEL:
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		sendMouseEvent = true;
		// fall through
	case WM_MOUSEMOVE:
		// thottle mouse move events
		if (wParam = WM_MOUSEMOVE && ++mouseMoveTick == MOUSE_DELAY)
		{
			sendMouseEvent = true;
			mouseMoveTick = 0;
		}

		if (sendMouseEvent)
		{
			Json::StyledWriter writer;
			Json::Value jmessage;
			jmessage["message"] = message;
			jmessage["wParam"] = wParam;
			jmessage["lParam"] = lParam;
			SendMouseInput(writer.write(jmessage));
		}

		// Mouse
		mouse_->ProcessMessage(message, wParam, lParam);

		auto mouse = mouse_->GetState();
		mouse_button_tracker_.Update(mouse);
		if (ball_camera_.IsDragging())
		{
			// Rotate camera
			ball_camera_.OnMove(mouse.x, mouse.y);
			Quaternion q = ball_camera_.GetQuat();
			q.Inverse(camera_rot_);
			if (++mouseTick == MOUSE_DELAY)
			{
				sendMessage = true;
				mouseTick = 0;
			}

			if (mouse_button_tracker_.leftButton == Mouse::ButtonStateTracker::RELEASED)
			{
				ball_camera_.OnEnd();
			}
		}
		else
		{
			// Zoom with scroll wheel
			float newZoom = 1.f + float(mouse.scrollWheelValue) / float(120 * 10);
			newZoom = std::max(newZoom, 0.01f);
			sendMessage |= newZoom != zoom_;
			zoom_ = newZoom;

			if (mouse_button_tracker_.leftButton == Mouse::ButtonStateTracker::PRESSED)
			{
				ball_camera_.OnBegin(mouse.x, mouse.y);
			}
		}

		break;

	default:
		return;
	}

	// Update camera
	Vector3 lookAt = Vector3::Transform(Vector3::Forward, camera_rot_);
	Vector3 up = Vector3::Transform(Vector3::Up, camera_rot_);
	last_camera_pos_ = camera_focus_ + (distance_ * zoom_) * lookAt;
	view_ = XMMatrixLookAtLH(last_camera_pos_, camera_focus_, up);
	if (sendMessage)
	{
		SendCameraInput(last_camera_pos_, camera_focus_, up);
	}
}

void Win32DataChannelHandler::ProcessMessage(MSG* msg)
{
	ProcessMessage(msg->message, msg->wParam, msg->lParam);
}

void Win32DataChannelHandler::ResetCamera()
{
	zoom_ = 1.f;
	camera_rot_ = Quaternion::Identity;
	camera_focus_ = Vector3::Zero;
	ball_camera_.Reset();
	mouse_->ResetScrollWheelValue();
	mouse_button_tracker_.Reset();
	Vector3 lookAt = Vector3::Transform(Vector3::Forward, camera_rot_);
	Vector3 up = Vector3::Transform(Vector3::Up, camera_rot_);
	last_camera_pos_ = camera_focus_ + (distance_ * zoom_) * lookAt;
}