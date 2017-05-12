#pragma once

#define CHECK_HR_FAILED(hr)							\
	if (FAILED(hr))									\
	{												\
		return;										\
	}

#define SAFE_RELEASE(p)								\
	if (p)											\
	{												\
		p->Release();								\
		p = nullptr;								\
	}
