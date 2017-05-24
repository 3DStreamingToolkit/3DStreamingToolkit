/*
*  DataChannelHandlerFactory contains interfaces used for creating the DataChannelHandler.
*/

#pragma once
#include "data_channel_handler.h"


class DataChannelHandlerFactory 
{
public:
	// Create a handler object
	// handlerType - name of the handler to instantiate
	static DataChannelHandler* Create(
		const char* handlerType);

};

