// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: reference additional headers your program requires here
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

#define STEREO_OUTPUT_MODE
#define IPD 0.02f
#define DEPLOYED_SERVICE
