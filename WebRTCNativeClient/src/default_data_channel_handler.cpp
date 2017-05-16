#include "pch.h"

#include "default_data_channel_handler.h"
#include "minwindef.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

DefaultDataChannelHandler::DefaultDataChannelHandler()
	: width_(0),
	height_(0),
	keyboardTick(0),
	mouseTick(0),
	zoom_(DEFAULT_ZOOM),
	distance_(DEFAULT_DISTANCE)
{
	mouse_ = std::make_unique<Mouse>();
}


DefaultDataChannelHandler::~DefaultDataChannelHandler()
{
}

bool DefaultDataChannelHandler::ProcessMessage(MSG* msg)
{
	bool ret = false;
	WPARAM wParam = msg->wParam;
	LPARAM lParam = msg->lParam;
	Vector3 move = Vector3::Zero;
	float scale = distance_;
	bool sendMessage = false;

	switch (msg->message)
	{
		case WM_SIZE:
			ball_camera_.SetWindow(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_CHAR:
		case WM_KEYDOWN:
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
		case WM_MOUSEHWHEEL:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			// Mouse
			mouse_->ProcessMessage(msg->message, wParam, lParam);

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
			return false;
	}

	// Update camera
	Vector3 lookAt = Vector3::Transform(Vector3::Forward, camera_rot_);
	Vector3 up = Vector3::Transform(Vector3::Up, camera_rot_);
	last_camera_pos_ = camera_focus_ + (distance_ * zoom_) * lookAt;
	view_ = XMMatrixLookAtLH(last_camera_pos_, camera_focus_, up);
	if (sendMessage)
	{
		char buffer[1024];
		sprintf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f",
			last_camera_pos_.x, last_camera_pos_.y, last_camera_pos_.z,
			camera_focus_.x, camera_focus_.y, camera_focus_.z,
			up.x, up.y, up.z);

		callback_->ProcessInput(std::string(buffer));
	}
	
	return ret;
}

void DefaultDataChannelHandler::ResetCamera()
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