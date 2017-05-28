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
 
#ifndef CUDAD3D11_H
#define CUDAD3D11_H

#if INIT_CUDA_D3D11

#include <d3d11.h>

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
#ifndef __CUDA_API_VERSION
    #define __CUDA_API_VERSION 3020
#endif
#endif /* CUDA_FORCE_API_VERSION */

#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION >= 3020
    #define cuD3D11CtxCreate cuD3D11CtxCreate_v2
#endif /* __CUDA_API_VERSION_INTERNAL || __CUDA_API_VERSION >= 3020 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup CUDA_D3D11 Direct3D 11 Interoperability
 * \ingroup CUDA_DRIVER
 *
 * ___MANBRIEF___ Direct3D 11 interoperability functions of the low-level CUDA
 * driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the Direct3D 11 interoperability functions of the
 * low-level CUDA driver application programming interface. Note that mapping 
 * of Direct3D 11 resources is performed with the graphics API agnostic, resource 
 * mapping interface described in \ref CUDA_GRAPHICS "Graphics Interoperability".
 *
 * @{
 */

/**
 * CUDA devices corresponding to a D3D11 device
 */
typedef enum CUd3d11DeviceList_enum {
    CU_D3D11_DEVICE_LIST_ALL            = 0x01, /**< The CUDA devices for all GPUs used by a D3D11 device */
    CU_D3D11_DEVICE_LIST_CURRENT_FRAME  = 0x02, /**< The CUDA devices for the GPUs used by a D3D11 device in its currently rendering frame */
    CU_D3D11_DEVICE_LIST_NEXT_FRAME     = 0x03, /**< The CUDA devices for the GPUs to be used by a D3D11 device in the next frame */
} CUd3d11DeviceList;

/**
 * \brief Gets the CUDA device corresponding to a display adapter.
 *
 * Returns in \p *pCudaDevice the CUDA-compatible device corresponding to the
 * adapter \p pAdapter obtained from ::IDXGIFactory::EnumAdapters.
 *
 * If no device on \p pAdapter is CUDA-compatible the call will return
 * ::CUDA_ERROR_NO_DEVICE.
 *
 * \param pCudaDevice - Returned CUDA device corresponding to \p pAdapter
 * \param pAdapter    - Adapter to query for CUDA device
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_NO_DEVICE,
 * ::CUDA_ERROR_INVALID_VALUE,
 * ::CUDA_ERROR_NOT_FOUND,
 * ::CUDA_ERROR_UNKNOWN
 * \notefnerr
 *
 * \sa
 * ::cuD3D11GetDevices
 */
typedef CUresult CUDAAPI tcuD3D11GetDevice(CUdevice *pCudaDevice, IDXGIAdapter *pAdapter);
extern tcuD3D11GetDevice *cuD3D11GetDevice;

/**
 * \brief Gets the CUDA devices corresponding to a Direct3D 11 device
 *
 * Returns in \p *pCudaDeviceCount the number of CUDA-compatible device corresponding
 * to the Direct3D 11 device \p pD3D11Device.
 * Also returns in \p *pCudaDevices at most \p cudaDeviceCount of the CUDA-compatible devices
 * corresponding to the Direct3D 11 device \p pD3D11Device.
 *
 * If any of the GPUs being used to render \p pDevice are not CUDA capable then the
 * call will return ::CUDA_ERROR_NO_DEVICE.
 *
 * \param pCudaDeviceCount - Returned number of CUDA devices corresponding to \p pD3D11Device
 * \param pCudaDevices     - Returned CUDA devices corresponding to \p pD3D11Device
 * \param cudaDeviceCount  - The size of the output device array \p pCudaDevices
 * \param pD3D11Device     - Direct3D 11 device to query for CUDA devices
 * \param deviceList       - The set of devices to return.  This set may be
 *                           ::CU_D3D11_DEVICE_LIST_ALL for all devices,
 *                           ::CU_D3D11_DEVICE_LIST_CURRENT_FRAME for the devices used to
 *                           render the current frame (in SLI), or
 *                           ::CU_D3D11_DEVICE_LIST_NEXT_FRAME for the devices used to
 *                           render the next frame (in SLI).
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_NO_DEVICE,
 * ::CUDA_ERROR_INVALID_VALUE,
 * ::CUDA_ERROR_NOT_FOUND,
 * ::CUDA_ERROR_UNKNOWN
 * \notefnerr
 *
 * \sa
 * ::cuD3D11GetDevice
 */
typedef CUresult CUDAAPI tcuD3D11GetDevices(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevices, unsigned int cudaDeviceCount, ID3D11Device *pD3D11Device, CUd3d11DeviceList deviceList);
extern tcuD3D11GetDevices *cuD3D11GetDevices;

/**
 * \brief Register a Direct3D 11 resource for access by CUDA
 *
 * Registers the Direct3D 11 resource \p pD3DResource for access by CUDA and
 * returns a CUDA handle to \p pD3Dresource in \p pCudaResource.
 * The handle returned in \p pCudaResource may be used to map and unmap this
 * resource until it is unregistered.
 * On success this call will increase the internal reference count on
 * \p pD3DResource. This reference count will be decremented when this
 * resource is unregistered through ::cuGraphicsUnregisterResource().
 *
 * This call is potentially high-overhead and should not be called every frame
 * in interactive applications.
 *
 * The type of \p pD3DResource must be one of the following.
 * - ::ID3D11Buffer: may be accessed through a device pointer.
 * - ::ID3D11Texture1D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D11Texture2D: individual subresources of the texture may be accessed via arrays
 * - ::ID3D11Texture3D: individual subresources of the texture may be accessed via arrays
 *
 * The \p Flags argument may be used to specify additional parameters at register
 * time.  The valid values for this parameter are
 * - ::CU_GRAPHICS_REGISTER_FLAGS_NONE: Specifies no hints about how this
 *   resource will be used.
 * - ::CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST: Specifies that CUDA will
 *   bind this resource to a surface reference.
 * - ::CU_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER: Specifies that CUDA will perform
 *   texture gather operations on this resource.
 *
 * Not all Direct3D resources of the above types may be used for
 * interoperability with CUDA.  The following are some limitations.
 * - The primary rendertarget may not be registered with CUDA.
 * - Resources allocated as shared may not be registered with CUDA.
 * - Textures which are not of a format which is 1, 2, or 4 channels of 8, 16,
 *   or 32-bit integer or floating-point data cannot be shared.
 * - Surfaces of depth or stencil formats cannot be shared.
 *
 * A complete list of supported DXGI formats is as follows. For compactness the
 * notation A_{B,C,D} represents A_B, A_C, and A_D.
 * - DXGI_FORMAT_A8_UNORM
 * - DXGI_FORMAT_B8G8R8A8_UNORM
 * - DXGI_FORMAT_B8G8R8X8_UNORM
 * - DXGI_FORMAT_R16_FLOAT
 * - DXGI_FORMAT_R16G16B16A16_{FLOAT,SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R16G16_{FLOAT,SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R16_{SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R32_FLOAT
 * - DXGI_FORMAT_R32G32B32A32_{FLOAT,SINT,UINT}
 * - DXGI_FORMAT_R32G32_{FLOAT,SINT,UINT}
 * - DXGI_FORMAT_R32_{SINT,UINT}
 * - DXGI_FORMAT_R8G8B8A8_{SINT,SNORM,UINT,UNORM,UNORM_SRGB}
 * - DXGI_FORMAT_R8G8_{SINT,SNORM,UINT,UNORM}
 * - DXGI_FORMAT_R8_{SINT,SNORM,UINT,UNORM}
 *
 * If \p pD3DResource is of incorrect type or is already registered then
 * ::CUDA_ERROR_INVALID_HANDLE is returned.
 * If \p pD3DResource cannot be registered then ::CUDA_ERROR_UNKNOWN is returned.
 * If \p Flags is not one of the above specified value then ::CUDA_ERROR_INVALID_VALUE
 * is returned.
 *
 * \param pCudaResource - Returned graphics resource handle
 * \param pD3DResource  - Direct3D resource to register
 * \param Flags         - Parameters for resource registration
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_INVALID_CONTEXT,
 * ::CUDA_ERROR_INVALID_VALUE,
 * ::CUDA_ERROR_INVALID_HANDLE,
 * ::CUDA_ERROR_OUT_OF_MEMORY,
 * ::CUDA_ERROR_UNKNOWN
 * \notefnerr
 *
 * \sa
 * ::cuGraphicsUnregisterResource,
 * ::cuGraphicsMapResources,
 * ::cuGraphicsSubResourceGetMappedArray,
 * ::cuGraphicsResourceGetMappedPointer
 */
typedef CUresult CUDAAPI tcuGraphicsD3D11RegisterResource(CUgraphicsResource *pCudaResource, ID3D11Resource *pD3DResource, unsigned int Flags);
extern tcuGraphicsD3D11RegisterResource *cuGraphicsD3D11RegisterResource;

/**
 * \defgroup CUDA_D3D11_DEPRECATED Direct3D 11 Interoperability [DEPRECATED]
 *
 * ___MANBRIEF___ deprecated Direct3D 11 interoperability functions of the
 * low-level CUDA driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes deprecated Direct3D 11 interoperability functionality.
 * @{
 */

#if __CUDA_API_VERSION >= 3020

/**
 * \brief Create a CUDA context for interoperability with Direct3D 11
 *
 * \deprecated This function is deprecated as of CUDA 5.0.
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a CUDA context with a D3D11
 * device in order to achieve maximum interoperability performance.
 *
 * \param pCtx        - Returned newly created CUDA context
 * \param pCudaDevice - Returned pointer to the device on which the context was created
 * \param Flags       - Context creation flags (see ::cuCtxCreate() for details)
 * \param pD3DDevice  - Direct3D device to create interoperability context with
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_INVALID_VALUE,
 * ::CUDA_ERROR_OUT_OF_MEMORY,
 * ::CUDA_ERROR_UNKNOWN
 * \notefnerr
 *
 * \sa
 * ::cuD3D11GetDevice,
 * ::cuGraphicsD3D11RegisterResource
 */
typedef CUresult CUDAAPI tcuD3D11CtxCreate(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, ID3D11Device *pD3DDevice);
typedef CUresult CUDAAPI tcuD3D11CtxCreate_v2(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, ID3D11Device *pD3DDevice);

extern tcuD3D11CtxCreate *cuD3D11CtxCreate;

/**
 * \brief Create a CUDA context for interoperability with Direct3D 11
 *
 * \deprecated This function is deprecated as of CUDA 5.0.
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a CUDA context with a D3D11
 * device in order to achieve maximum interoperability performance.
 *
 * \param pCtx        - Returned newly created CUDA context
 * \param flags       - Context creation flags (see ::cuCtxCreate() for details)
 * \param pD3DDevice  - Direct3D device to create interoperability context with
 * \param cudaDevice  - The CUDA device on which to create the context.  This device
 *                      must be among the devices returned when querying
 *                      ::CU_D3D11_DEVICES_ALL from  ::cuD3D11GetDevices.
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_INVALID_VALUE,
 * ::CUDA_ERROR_OUT_OF_MEMORY,
 * ::CUDA_ERROR_UNKNOWN
 * \notefnerr
 *
 * \sa
 * ::cuD3D11GetDevices,
 * ::cuGraphicsD3D11RegisterResource
 */

typedef CUresult CUDAAPI tcuD3D11CtxCreateOnDevice(CUcontext *pCtx, unsigned int flags, ID3D11Device *pD3DDevice, CUdevice cudaDevice);
extern tcuD3D11CtxCreateOnDevice *cuD3D11CtxCreateOnDevice;

/**
 * \brief Get the Direct3D 11 device against which the current CUDA context was
 * created
 *
 * \deprecated This function is deprecated as of CUDA 5.0.
 *
 * This function is deprecated and should no longer be used.  It is
 * no longer necessary to associate a CUDA context with a D3D11
 * device in order to achieve maximum interoperability performance.
 *
 * \param ppD3DDevice - Returned Direct3D device corresponding to CUDA context
 *
 * \return
 * ::CUDA_SUCCESS,
 * ::CUDA_ERROR_DEINITIALIZED,
 * ::CUDA_ERROR_NOT_INITIALIZED,
 * ::CUDA_ERROR_INVALID_CONTEXT
 * \notefnerr
 *
 * \sa
 * ::cuD3D11GetDevice
 */
typedef CUresult CUDAAPI tcuD3D11GetDirect3DDevice(ID3D11Device **ppD3DDevice);
extern tcuD3D11GetDirect3DDevice *cuD3D11GetDirect3DDevice;

#endif /* __CUDA_API_VERSION >= 3020 */

/** @} */ /* END CUDA_D3D11_DEPRECATED */
/** @} */ /* END CUDA_D3D11 */

/**
 * CUDA API versioning support
 */
#if defined(__CUDA_API_VERSION_INTERNAL)
    #undef cuD3D11CtxCreate
#endif /* __CUDA_API_VERSION_INTERNAL */

/**
 * CUDA API made obselete at API version 3020
 */
#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION < 3020
CUresult CUDAAPI cuD3D11CtxCreate(CUcontext *pCtx, CUdevice *pCudaDevice, unsigned int Flags, ID3D11Device *pD3DDevice);
#endif /* __CUDA_API_VERSION_INTERNAL || __CUDA_API_VERSION < 3020 */

#ifdef __cplusplus
};
#endif

#endif

#endif

