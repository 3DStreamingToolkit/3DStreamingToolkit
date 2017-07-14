#pragma once

using namespace DirectX::SimpleMath;

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
};
