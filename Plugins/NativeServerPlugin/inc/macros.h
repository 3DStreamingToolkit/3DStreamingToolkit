#pragma once

#define CHECK_NV_FAILED(nv) if (nv != NV_ENC_SUCCESS) { return nv; }
#define CHECK_HR_FAILED(hr) if (FAILED(hr)) { return; }

#define SAFE_RELEASE(p)					\
	if (p)								\
	{									\
		p->Release();					\
		p = nullptr;					\
	}

#define FORWARD_DECLARATION(input_namespace, input_class)	\
namespace input_namespace									\
{															\
	class input_class; 										\
}

#define FRIEND_CLASS(test_namespace, test_class) friend class test_namespace##::##test_class
