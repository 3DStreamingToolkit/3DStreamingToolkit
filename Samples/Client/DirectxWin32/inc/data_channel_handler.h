#pragma once

using namespace DirectX::SimpleMath;

class DataChannelCallback
{
public:
	virtual void SendInputData(const std::string&) = 0;
};

class DataChannelHandler
{
protected:
	DataChannelHandler(DataChannelCallback* data_channel_callback);

	~DataChannelHandler();

	void SendCameraInput(
		Vector3 camera_position,
		Vector3 camera_target,
		Vector3 camera_up_vector);

	void SendCameraInput(float x, float y, float z, float yaw, float pitch, float roll);

	void SendKeyboardInput(const std::string& msg);

	void SendMouseInput(const std::string& msg);

private:
	DataChannelCallback* data_channel_callback_;
};
