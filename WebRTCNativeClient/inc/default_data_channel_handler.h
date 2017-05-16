/*
*  DefaultDataChannelHandler applies mouse and keyboard messages to calculate the
*  camera state and forwards updates to the camera state to the data channel with
*  a text message containing camera position, look at position, and up vector.
*
*  ArcBall is used to apply orientation changes from mouse events.
*
*  Arrow keys and 'A' 'W' 'D' 'S' keys are used to position the camera.
*/

#pragma once
#include "data_channel_handler.h"
#include "arc_ball.h"



class DefaultDataChannelHandler : public DataChannelHandler
{
public:
	DefaultDataChannelHandler();
	~DefaultDataChannelHandler();

	virtual bool ProcessMessage(MSG* msg) override;

private:
	void ResetCamera();

	int width_;
	int height_;
	int keyboardTick;
	int mouseTick;
	std::unique_ptr<DirectX::Mouse> mouse_;
	DirectX::Mouse::ButtonStateTracker mouse_button_tracker_;
	ArcBall ball_camera_;
	DirectX::SimpleMath::Matrix view_;
	DirectX::SimpleMath::Vector3 camera_focus_;
	DirectX::SimpleMath::Vector3 last_camera_pos_;
	DirectX::SimpleMath::Quaternion camera_rot_;
	float zoom_;
	float distance_;
};

