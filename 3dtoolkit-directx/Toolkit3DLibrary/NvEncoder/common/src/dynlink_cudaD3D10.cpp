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

#include <stdio.h>
#include "../inc/dynlink_cudaD3D10.h"

// D3D10/CUDA interop (CUDA 3.0+)
#if __CUDA_API_VERSION >= 3020
tcuD3D10GetDevice                     *cuD3D10GetDevice;
tcuD3D10CtxCreate                     *cuD3D10CtxCreate;
tcuGraphicsD3D10RegisterResource      *cuGraphicsD3D10RegisterResource;
#endif

tcuD3D10RegisterResource              *cuD3D10RegisterResource;
tcuD3D10UnregisterResource            *cuD3D10UnregisterResource;
tcuD3D10MapResources                  *cuD3D10MapResources;
tcuD3D10UnmapResources                *cuD3D10UnmapResources;
tcuD3D10ResourceSetMapFlags           *cuD3D10ResourceSetMapFlags;
tcuD3D10ResourceGetMappedArray        *cuD3D10ResourceGetMappedArray;

#if __CUDA_API_VERSION >= 3020
tcuD3D10ResourceGetMappedPointer      *cuD3D10ResourceGetMappedPointer;
tcuD3D10ResourceGetMappedSize         *cuD3D10ResourceGetMappedSize;
tcuD3D10ResourceGetMappedPitch        *cuD3D10ResourceGetMappedPitch;
tcuD3D10ResourceGetSurfaceDimensions  *cuD3D10ResourceGetSurfaceDimensions;
#endif /* __CUDA_API_VERSION >= 3020 */

#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION < 3020
tcuD3D10CtxCreate                     *cuD3D10CtxCreate;
tcuD3D10ResourceGetMappedPitch        *cuD3D10ResourceGetMappedPitch;
tcuD3D10ResourceGetMappedPointer      *cuD3D10ResourceGetMappedPointer;
tcuD3D10ResourceGetMappedSize         *cuD3D10ResourceGetMappedSize;
tcuD3D10ResourceGetSurfaceDimensions  *cuD3D10ResourceGetSurfaceDimensions;
#endif /* __CUDA_API_VERSION_INTERNAL || __CUDA_API_VERSION < 3020 */

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
        return CUDA_ERROR_UNKNOWN;                                      \
    }

#define GET_PROC_EX_V2(name, alias, required)                           \
    alias = (t##name *)GetProcAddress(CudaDrvLib, STRINGIFY(name##_v2));\
    if (alias == NULL && required) {                                    \
        printf("Failed to find required function \"%s\" in %s\n",       \
               STRINGIFY(name##_v2), __CudaLibName);                       \
        return CUDA_ERROR_UNKNOWN;                                      \
    }

#elif defined(__unix__) || defined(__APPLE__) || defined(__MACOSX)

#include <dlfcn.h>

typedef void *CUDADRIVER;

#if defined(__APPLE__) || defined(__MACOSX)
static char __CudaLibName[] = "/usr/local/cuda/lib/libcuda.dylib";
#else
static char __CudaLibName[] = "libcuda.so";
#endif

#define GET_PROC_EX(name, alias, required)                              \
    alias = (t##name *)dlsym(CudaDrvLib, #name);                        \
    if (alias == NULL && required) {                                    \
        printf("Failed to find required function \"%s\" in %s\n",       \
               #name, __CudaLibName);                                  \
        return CUDA_ERROR_UNKNOWN;                                      \
    }

#define GET_PROC_EX_V2(name, alias, required)                           \
    alias = (t##name *)dlsym(CudaDrvLib, STRINGIFY(name##_v2));         \
    if (alias == NULL && required) {                                    \
        printf("Failed to find required function \"%s\" in %s\n",       \
               STRINGIFY(name##_v2), __CudaLibName);                    \
        return CUDA_ERROR_UNKNOWN;                                      \
    }

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

// Prior to calling cuInitD3D10, CudaDrvLib must be loaded, and cuInit must be called
CUresult CUDAAPI cuInitD3D10(unsigned int Flags, int cudaVersion, CUDADRIVER &CudaDrvLib)
{
    // fetch all function pointers
    GET_PROC(cuD3D10GetDevice);
    GET_PROC(cuD3D10CtxCreate);
    GET_PROC(cuGraphicsD3D10RegisterResource);

    return CUDA_SUCCESS;
}
