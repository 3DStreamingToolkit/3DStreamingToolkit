/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */
 
#ifndef CUDAD3D10_H
#define CUDAD3D10_H

#if INIT_CUDA_D3D

/**
 * CUDA API versioning support
 */
#if defined(CUDA_FORCE_API_VERSION)
    #if (CUDA_FORCE_API_VERSION == 3010)
        #define __CUDA_API_VERSION 3010
    #else
        #error "Unsupported value of CUDA_FORCE_API_VERSION"
    #endif
#else
    #define __CUDA_API_VERSION 3020
#endif /* CUDA_FORCE_API_VERSION */

#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION >= 3020
    #define cuD3D10CtxCreate                    cuD3D10CtxCreate_v2
    #define cuD3D10ResourceGetSurfaceDimensions cuD3D10ResourceGetSurfaceDimensions_v2
    #define cuD3D10ResourceGetMappedPointer     cuD3D10ResourceGetMappedPointer_v2
    #define cuD3D10ResourceGetMappedSize        cuD3D10ResourceGetMappedSize_v2
    #define cuD3D10ResourceGetMappedPitch       cuD3D10ResourceGetMappedPitch_v2
#endif /* __CUDA_API_VERSION_INTERNAL || __CUDA_API_VERSION >= 3020 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup CUDA_D3D10 Direct3D 10 Interoperability
 * \ingroup CUDA_DRIVER
 *
 * ___MANBRIEF___ Direct3D 10 interoperability functions of the low-level CUDA
 * driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the Direct3D 10 interoperability functions of the
 * low-level CUDA driver application programming interface. Note that mapping 
 * of Direct3D 10 resources is performed with the graphics API agnostic, resource 
 * mapping interface described in \ref CUDA_GRAPHICS "Graphics Interoperability".
 *
 * @{
 */

/**
 * CUDA devices corresponding to a D3D10 device
 */
typedef enum CUd3d10DeviceList_enum {
    CU_D3D10_DEVICE_LIST_ALL            = 0x01, /**< The CUDA devices for all GPUs used by a D3D10 device */
    CU_D3D10_DEVICE_LIST_CURRENT_FRAME  = 0x02, /**< The CUDA devices for the GPUs used by a D3D10 device in its currently rendering frame */
    CU_D3D10_DEVICE_LIST_NEXT_FRAME     = 0x03, /**< The CUDA devices for the GPUs to be used by a D3D10 device in the next frame */
} CUd3d10DeviceList;

typedef CUresult CUDAAPI tcuD3D10GetDevice(CUdevice *pCudaDevice, IDXGIAdapter *pAdapter);
typedef CUresult CUDAAPI tcuD3D10GetDevices(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevices, unsigned int cudaDeviceCount, ID3D10Device *pD3D10Device, CUd3d10DeviceList deviceList);
typedef CUresult CUDAAPI tcuGraphicsD3D10RegisterResource(CUgraphicsResource *pCudaResource, ID3D10Resource *pD3DResource, unsigned int Flags);

/**
 * \defgroup CUDA_D3D10_DEPRECATED Direct3D 10 Interoperability [DEPRECATED]
 *
 * ___MANBRIEF___ deprecated Direct3D 10 interoperability functions of the 
 * low-level CUDA driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes deprecated Direct3D 10 interoperability functionality.
 * @{
 */

/** Flags to register a resource */
typedef enum CUD3D10register_flags_enum {
    CU_D3D10_REGISTER_FLAGS_NONE  = 0x00,
    CU_D3D10_REGISTER_FLAGS_ARRAY = 0x01,
} CUD3D10register_flags;

/** Flags to map or unmap a resource */
typedef enum CUD3D10map_flags_enum {
    CU_D3D10_MAPRESOURCE_FLAGS_NONE         = 0x00,
    CU_D3D10_MAPRESOURCE_FLAGS_READONLY     = 0x01,
    CU_D3D10_MAPRESOURCE_FLAGS_WRITEDISCARD = 0x02,
} CUD3D10map_flags;


#if __CUDA_API_VERSION >= 3020
    typedef CUresult CUDAAPI tcuD3D10CtxCreate(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, ID3D10Device *pD3DDevice);
    typedef CUresult CUDAAPI tcuD3D10CtxCreateOnDevice(CUcontext *pCtx, unsigned int flags, ID3D10Device *pD3DDevice, CUdevice cudaDevice);
    typedef CUresult CUDAAPI tcuD3D10GetDirect3DDevice(ID3D10Device **ppD3DDevice);
#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuD3D10RegisterResource(ID3D10Resource *pResource, unsigned int Flags);
typedef CUresult CUDAAPI tcuD3D10UnregisterResource(ID3D10Resource *pResource);
typedef CUresult CUDAAPI tcuD3D10MapResources(unsigned int count, ID3D10Resource **ppResources);
typedef CUresult CUDAAPI tcuD3D10UnmapResources(unsigned int count, ID3D10Resource **ppResources);
typedef CUresult CUDAAPI tcuD3D10ResourceSetMapFlags(ID3D10Resource *pResource, unsigned int Flags);
typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedArray(CUarray *pArray, ID3D10Resource *pResource, unsigned int SubResource);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedPointer(CUdeviceptr *pDevPtr, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedSize(size_t *pSize, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedPitch(size_t *pPitch, size_t *pPitchSlice, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetSurfaceDimensions(size_t *pWidth, size_t *pHeight, size_t *pDepth, ID3D10Resource *pResource, unsigned int SubResource);
#endif /* __CUDA_API_VERSION >= 3020 */

/** @} */ /* END CUDA_D3D10_DEPRECATED */
/** @} */ /* END CUDA_D3D10 */

/**
 * CUDA API versioning support
 */
#if defined(__CUDA_API_VERSION_INTERNAL)
    #undef cuD3D10CtxCreate
    #undef cuD3D10ResourceGetSurfaceDimensions
    #undef cuD3D10ResourceGetMappedPointer
    #undef cuD3D10ResourceGetMappedSize
    #undef cuD3D10ResourceGetMappedPitch
#endif /* __CUDA_API_VERSION_INTERNAL */

/**
 * CUDA API made obselete at API version 3020
 */
#if defined(__CUDA_API_VERSION_INTERNAL)
    #define CUdeviceptr CUdeviceptr_v1
#endif /* __CUDA_API_VERSION_INTERNAL */

#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION < 3020
    typedef CUresult CUDAAPI tcuD3D10CtxCreate(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, ID3D10Device *pD3DDevice);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedPitch(unsigned int *pPitch, unsigned int *pPitchSlice, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedPointer(CUdeviceptr *pDevPtr, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetMappedSize(unsigned int *pSize, ID3D10Resource *pResource, unsigned int SubResource);
    typedef CUresult CUDAAPI tcuD3D10ResourceGetSurfaceDimensions(unsigned int *pWidth, unsigned int *pHeight, unsigned int *pDepth, ID3D10Resource *pResource, unsigned int SubResource);
#endif /* __CUDA_API_VERSION_INTERNAL || __CUDA_API_VERSION < 3020 */

#if defined(__CUDA_API_VERSION_INTERNAL)
    #undef CUdeviceptr
#endif /* __CUDA_API_VERSION_INTERNAL */

#ifdef __cplusplus
};
#endif

extern tcuD3D10GetDevice                     *cuD3D10GetDevice;
extern tcuD3D10CtxCreate                     *cuD3D10CtxCreate;
extern tcuGraphicsD3D10RegisterResource      *cuGraphicsD3D10RegisterResource;

extern tcuD3D10RegisterResource              *cuD3D10RegisterResource;
extern tcuD3D10UnregisterResource            *cuD3D10UnregisterResource;
extern tcuD3D10MapResources                  *cuD3D10MapResources;
extern tcuD3D10UnmapResources                *cuD3D10UnmapResources;
extern tcuD3D10ResourceSetMapFlags           *cuD3D10ResourceSetMapFlags;
extern tcuD3D10ResourceGetMappedArray        *cuD3D10ResourceGetMappedArray;

#if (__CUDA_API_VERSION >= 3020)
extern tcuD3D10ResourceGetMappedPointer      *cuD3D10ResourceGetMappedPointer;
extern tcuD3D10ResourceGetMappedSize         *cuD3D10ResourceGetMappedSize;
extern tcuD3D10ResourceGetMappedPitch        *cuD3D10ResourceGetMappedPitch;
extern tcuD3D10ResourceGetSurfaceDimensions  *cuD3D10ResourceGetSurfaceDimensions;
#endif /* __CUDA_API_VERSION >= 3020 */

#if (__CUDA_API_VERSION < 3020)
extern tcuD3D10CtxCreate                     *cuD3D10CtxCreate;
extern tcuD3D10ResourceGetMappedPitch        *cuD3D10ResourceGetMappedPitch;
extern tcuD3D10ResourceGetMappedPointer      *cuD3D10ResourceGetMappedPointer;
extern tcuD3D10ResourceGetMappedSize         *cuD3D10ResourceGetMappedSize;
extern tcuD3D10ResourceGetSurfaceDimensions  *cuD3D10ResourceGetSurfaceDimensions;
#endif /* __CUDA_API_VERSION < 3020 */

#endif

#endif
