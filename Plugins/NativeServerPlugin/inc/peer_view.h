#pragma once

#include "pch.h"
#include <DirectXMath.h>

namespace StreamingToolkit
{
	class PeerView
	{
	public:

		// dirextx vectors must be 16byte aligned
		// so we override the operators to support
		// new-ing this class

		void* operator new(size_t i);

		void operator delete(void* p);

		// determines if the data structure is valid
		// for reading/using
		bool IsValid();

		DirectX::XMVECTORF32 lookAt;
		DirectX::XMVECTORF32 up;
		DirectX::XMVECTORF32 eye;
	};
}