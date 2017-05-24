#include "pch.h"

#include "data_channel_handler_factory.h"
#include "json_data_channel_handler.h"
#include "default_data_channel_handler.h"

DataChannelHandler* DataChannelHandlerFactory::Create(
	const char* handlerType)
{
	if (strcmp(handlerType, "JSON") == 0)
	{
		return new JsonDataChannelHandler();
	}

	return new DefaultDataChannelHandler();
}
