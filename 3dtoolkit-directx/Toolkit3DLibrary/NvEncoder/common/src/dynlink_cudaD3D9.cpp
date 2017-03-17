/**
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

// With these flags defined, this source file will dynamically
// load the corresponding functions.  Disabled by default.

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#endif
#include <d3dx9.h>

#include <stdio.h>
#include "../inc/dynlink_cuda.h"
#include "../inc/dynlink_cudaD3D9.h"

// D3D9/CUDA interop (CUDA 1.x compatible API). These functions
// are deprecated; please use the ones below
tcuD3D9Begin                          *cuD3D9Begin;
tcuD3D9End                            *cuD3D9End;

// D3D9/CUDA interop (CUDA 2.x compatible)
tcuD3D9GetDirect3DDevice              *cuD3D9GetDirect3DDevice;
tcuD3D9RegisterResource               *cuD3D9RegisterResource;
tcuD3D9UnregisterResource             *cuD3D9UnregisterResource;
tcuD3D9MapResources                   *cuD3D9MapResources;
tcuD3D9UnmapResources                 *cuD3D9UnmapResources;
tcuD3D9ResourceSetMapFlags            *cuD3D9ResourceSetMapFlags;
tcuD3D9ResourceGetSurfaceDimensions   *cuD3D9ResourceGetSurfaceDimensions;
tcuD3D9ResourceGetMappedArray         *cuD3D9ResourceGetMappedArray;
tcuD3D9ResourceGetMappedPointer       *cuD3D9ResourceGetMappedPointer;
tcuD3D9ResourceGetMappedSize          *cuD3D9ResourceGetMappedSize;
tcuD3D9ResourceGetMappedPitch         *cuD3D9ResourceGetMappedPitch;

// D3D9/CUDA interop (CUDA 2.0+)
tcuD3D9GetDevice                      *cuD3D9GetDevice;
tcuD3D9GetDevice                      *cuD3D9GetDevices;
tcuD3D9GetDevice                      *cuD3D9GetDevice_v2;
tcuD3D9CtxCreate                      *cuD3D9CtxCreate;
tcuD3D9CtxCreate                      *cuD3D9CtxCreate_v2;
tcuGraphicsD3D9RegisterResource       *cuGraphicsD3D9RegisterResource;
tcuGraphicsD3D9RegisterResource       *cuGraphicsD3D9RegisterResource_v2;

#define STRINGIFY(X) #X

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <Windows.h>

typedef HMODULE CUDADRIVER;

#ifdef UNICODE
static LPCWSTR __CudaLibName = L"nvcuda.dll";
#else
static LPCSTR __CudaLibName = "nvcuda.dll";
#endif

#define GET_PROC_EX(name, alias, required)                     \
    alias = (t##name *)GetProcAddress(CudaDrvLib, #name);               \
    if (alias == NULL && required) {                                    \
        printf("Failed to find required function \"%s\" in %s\n",       \
               #name, __CudaLibName);                                  \
        }
//        return CUDA_ERROR_UNKNOWN;                                      \

#define GET_PROC_EX_V2(name, alias, required)                           \
    alias = (t##name *)GetProcAddress(CudaDrvLib, STRINGIFY(name##_v2));\
    if (alias == NULL && required) {                                    \
        printf("Failed to find required function \"%s\" in %s\n",       \
               STRINGIFY(name##_v2), __CudaLibName);                       \
        }
//        return CUDA_ERROR_UNKNOWN;                                      \

#else
    #error unsupported platform
#endif

#define CHECKED_CALL(call)              \
    do {                                \
        CUresult result = (call);       \
        if (CUDA_SUCCESS != result) {   \
            return result;              \
        }                               \
    } while(0)

#define GET_PROC_REQUIRED(name) GET_PROC_EX(name,name,1)
#define GET_PROC_OPTIONAL(name) GET_PROC_EX(name,name,0)
#define GET_PROC(name)          GET_PROC_REQUIRED(name)
#define GET_PROC_V2(name)       GET_PROC_EX_V2(name,name,1)

// Prior to calling cuInitD3D9, CudaDrvLib must be loaded, and cuInit must be called
CUresult CUDAAPI cuInitD3D9(unsigned int Flags, int cudaVersion, CUDADRIVER &CudaDrvLib)
{
    // D3D9/CUDA (CUDA 1.x compatible API)
    GET_PROC(cuD3D9Begin);
    GET_PROC(cuD3D9End);

    // D3D9/CUDA (CUDA 2.x compatible API)
    GET_PROC(cuD3D9GetDirect3DDevice);
    GET_PROC(cuD3D9RegisterResource);
    GET_PROC(cuD3D9UnregisterResource);
    GET_PROC(cuD3D9MapResources);
    GET_PROC(cuD3D9UnmapResources);
    GET_PROC(cuD3D9ResourceSetMapFlags);

    // D3D9/CUDA (CUDA 2.0+ compatible API)
    GET_PROC(cuD3D9GetDevice);
    GET_PROC(cuGraphicsD3D9RegisterResource);

    GET_PROC_V2(cuD3D9CtxCreate);
    GET_PROC_V2(cuD3D9ResourceGetSurfaceDimensions);
    GET_PROC_V2(cuD3D9ResourceGetMappedPointer);
    GET_PROC_V2(cuD3D9ResourceGetMappedSize);
    GET_PROC_V2(cuD3D9ResourceGetMappedPitch);
    GET_PROC_V2(cuD3D9ResourceGetMappedArray);

    return CUDA_SUCCESS;
}
