#pragma once

// DirectXTK
#include <GamePad.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

// For unit tests.
FOWARD_DECLARE(EndToEndTests, SingleClientToServer);
FOWARD_DECLARE(EndToEndTests, DISABLED_SingleClientToServer);
FOWARD_DECLARE(EndToEndTests, ServerToClientLatency);
FOWARD_DECLARE(EndToEndTests, DISABLED_ServerToClientLatency);

class DataChannelCallback
{
public:
	virtual bool SendInputData(const std::string&) = 0;
};

class DataChannelHandler
{
protected:
	DataChannelHandler(DataChannelCallback* data_channel_callback);

	~DataChannelHandler();

	bool SendCameraInput(
		Vector3 camera_position,
		Vector3 camera_target,
		Vector3 camera_up_vector);

	bool SendCameraInput(float x, float y, float z, float yaw, float pitch, float roll);

	bool SendKeyboardInput(const std::string& msg);

	bool SendMouseInput(const std::string& msg);

	bool RequestStereoStream(bool stereo);

private:
	DataChannelCallback* data_channel_callback_;

	// For unit tests.
	FRIEND_TEST(EndToEndTests, SingleClientToServer);
	FRIEND_TEST(EndToEndTests, DISABLED_SingleClientToServer);
	FRIEND_TEST(EndToEndTests, ServerToClientLatency);
	FRIEND_TEST(EndToEndTests, DISABLED_ServerToClientLatency);
};
