#pragma once

#define CHECK_NV_FAILED(nv) if (nv != NV_ENC_SUCCESS) { return nv; }
#define CHECK_HR_FAILED(hr) if (FAILED(hr)) { return; }
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)					\
	if (p)								\
	{									\
		p->Release();					\
		p = nullptr;					\
	}
#endif
