#pragma once
#include "main_window.h"
#include "webrtc/base/win32.h"

class DataChannelHandler
{
public:
	DataChannelHandler();
	~DataChannelHandler();

	void SetCallback(MainWindowCallback* callback);

	virtual bool ProcessMessage(MSG* msg) = 0;

protected:
	MainWindowCallback* callback_;
};

