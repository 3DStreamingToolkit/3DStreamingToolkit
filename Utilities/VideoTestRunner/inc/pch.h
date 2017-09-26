#pragma once

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

// This requires DirectX 11.4
#define MULTITHREAD_PROTECTION
#define MAX_ENCODE_QUEUE 32
#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024

#include "macros.h"