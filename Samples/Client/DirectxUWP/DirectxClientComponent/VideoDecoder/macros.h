#pragma once

#define CHECK_HR(hr, msg)					\
	if (hr != S_OK)							\
	{										\
		printf(msg);						\
		printf("Error: %.2X.\n", hr);		\
	}
