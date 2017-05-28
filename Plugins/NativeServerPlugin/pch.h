#pragma once

// Windows headers
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <stdio.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>

#ifdef _WIN32
#include <io.h> 
#define access    _access_s
#else
#include <unistd.h>
#endif

#include "macros.h"

// WebRTC conversion from 'uint64_t' to 'uint32_t', possible loss of data
#pragma warning(disable : 4244)
