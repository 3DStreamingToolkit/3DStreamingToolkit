#include "pch.h"
#include "data_channel_handler.h"


DataChannelHandler::DataChannelHandler()
	: callback_(NULL)
{
}

DataChannelHandler::~DataChannelHandler()
{
}

void DataChannelHandler::SetCallback(MainWindowCallback* callback)
{
	callback_ = callback;
}