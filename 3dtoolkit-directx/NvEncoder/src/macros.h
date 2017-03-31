#pragma once

#define SAFE_RELEASE(p)					\
	if (p)								\
	{									\
		p->Release();					\
		p = nullptr;					\
	}

