#include "pch.h"
#include "data_channel_handler.h"
#include "webrtc/base/json.h"

// Data channel message types.
const char kCameraTransformLookAtMsgType[]		= "camera-transform-lookat";
const char kCameraTransformMsgType[]			= "camera-transform";
const char kKeyboardEventMsgType[]				= "keyboard-event";
const char kMouseEventMsgType[]					= "mouse-event";

DataChannelHandler::DataChannelHandler(DataChannelCallback* data_channel_callback) :
	_data_channel_callback(data_channel_callback)
{
}

DataChannelHandler::~DataChannelHandler()
{
}

void DataChannelHandler::SendCameraInput(
	Vector3 camera_position,
	Vector3 camera_target,
	Vector3 camera_up_vector)
{
	char buffer[1024];
	sprintf(buffer, "%f, %f, %f, %f, %f, %f, %f, %f, %f",
		camera_position.x, camera_position.y, camera_position.z,
		camera_target.x, camera_target.y, camera_target.z,
		camera_up_vector.x, camera_up_vector.y, camera_up_vector.z);

	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = kCameraTransformLookAtMsgType;
	jmessage["body"] = buffer;

	_data_channel_callback->SendInputData(writer.write(jmessage));
}

void DataChannelHandler::SendCameraInput(
	float x, float y, float z, float yaw, float pitch, float roll)
{
	char buffer[1024];
	sprintf(buffer, "%f, %f, %f, %f, %f, %f",
		x, y, z, yaw, pitch, roll);

	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = kCameraTransformMsgType;
	jmessage["body"] = buffer;

	_data_channel_callback->SendInputData(writer.write(jmessage));
}

void DataChannelHandler::SendKeyboardInput(const std::string& msg)
{
	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = kKeyboardEventMsgType;
	jmessage["body"] = msg;
	_data_channel_callback->SendInputData(writer.write(jmessage));
}

void DataChannelHandler::SendMouseInput(const std::string& msg)
{
	Json::StyledWriter writer;
	Json::Value jmessage;
	jmessage["type"] = kMouseEventMsgType;
	jmessage["body"] = msg;
	_data_channel_callback->SendInputData(writer.write(jmessage));
}
