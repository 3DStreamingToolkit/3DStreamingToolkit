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

#ifndef ENABLE_TEST
#define FOWARD_DECLARE(test_case_name, test_name)
#define FRIEND_TEST(test_case_name, test_name)
#else // ENABLE_TEST
#define FOWARD_DECLARE(test_case_name, test_name) class test_case_name##_##test_name##_Test
#define FRIEND_TEST(test_case_name, test_name) friend class test_case_name##_##test_name##_Test
#endif // ENABLE_TEST
