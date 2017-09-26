#pragma once

// Windows headers
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <wrl/client.h>
#include <wchar.h>
#include <math.h>

// DirectX
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>

// DirectXTK
#include <GamePad.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <SimpleMath.h>

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