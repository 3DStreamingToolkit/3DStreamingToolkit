////////////////////////////////////////////////////////////////////////////
//
// Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
//
// Please refer to the NVIDIA end user license agreement (EULA) associated
// with this source code for terms and conditions that govern your use of
// this software. Any use, reproduction, disclosure, or distribution of
// this software and related documentation outside the terms of the EULA
// is strictly prohibited.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <d3d9.h>
#include <Dxva2api.h>
#include "../common/inc/nvEncodeAPI.h"
#include "../common/inc/nvCPUOPSys.h"
#include "../common/inc/NvHWEncoder.h"
#pragma warning(disable : 4996)

#define MAX_ENCODE_QUEUE 32

#define SET_VER(configStruct, type) {configStruct.version = type##_VER;}
#define NVENCAPI __stdcall

template<class T>
class CNvQueue {
    T** m_pBuffer;
    unsigned int m_uSize;
    unsigned int m_uPendingCount;
    unsigned int m_uAvailableIdx;
    unsigned int m_uPendingndex;
public:
    CNvQueue(): m_uSize(0), m_uPendingCount(0), m_uAvailableIdx(0),
                m_uPendingndex(0), m_pBuffer(NULL)
    {
    }

    ~CNvQueue()
    {
        delete[] m_pBuffer;
    }

    bool Initialize(T *pItems, unsigned int uSize)
    {
        m_uSize = uSize;
        m_uPendingCount = 0;
        m_uAvailableIdx = 0;
        m_uPendingndex = 0;
        m_pBuffer = new T *[m_uSize];
        for (unsigned int i = 0; i < m_uSize; i++)
        {
            m_pBuffer[i] = &pItems[i];
        }
        return true;
    }


    T * GetAvailable()
    {
        T *pItem = NULL;
        if (m_uPendingCount == m_uSize)
        {
            return NULL;
        }
        pItem = m_pBuffer[m_uAvailableIdx];
        m_uAvailableIdx = (m_uAvailableIdx+1)%m_uSize;
        m_uPendingCount += 1;
        return pItem;
    }

    T* GetPending()
    {
        if (m_uPendingCount == 0) 
        {
            return NULL;
        }

        T *pItem = m_pBuffer[m_uPendingndex];
        m_uPendingndex = (m_uPendingndex+1)%m_uSize;
        m_uPendingCount -= 1;
        return pItem;
    }
};

typedef struct _EncodeFrameConfig
{
    IDirect3DTexture9 *pRGBTexture;
    uint32_t width;
    uint32_t height;
}EncodeFrameConfig;

class CNvEncoderD3DInterop
{
public:
    CNvEncoderD3DInterop();
    virtual ~CNvEncoderD3DInterop();
    int                                                  EncodeMain(int argc, char **argv);

protected:
    CNvHWEncoder                                        *m_pNvHWEncoder;
    uint32_t                                             m_uEncodeBufferCount;
    EncodeOutputBuffer                                   m_stEOSOutputBfr;
    IDirect3D9Ex                                        *m_pD3DEx;
    IDirect3DDevice9                                    *m_pD3D9Device;
    IDirectXVideoProcessorService                       *m_pDXVA2VideoProcessServices;
    IDirectXVideoProcessor                              *m_pDXVA2VideoProcessor;
    DXVA2_ValueRange                                     m_Brightness;
    DXVA2_ValueRange                                     m_Contrast;
    DXVA2_ValueRange                                     m_Hue;
    DXVA2_ValueRange                                     m_Saturation;

    EncodeBuffer                                         m_stEncodeBuffer[MAX_ENCODE_QUEUE];
    CNvQueue<EncodeBuffer>                               m_EncodeBufferQueue;

    uint8_t                                             *m_yuv[3];

protected:
    NVENCSTATUS                                          Deinitialize();
    NVENCSTATUS                                          InitD3D9(unsigned int deviceID=0);
    NVENCSTATUS                                          AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight);
    NVENCSTATUS                                          ReleaseIOBuffers();
    NVENCSTATUS                                          FlushEncoder();
    NVENCSTATUS                                          ConvertRGBToNV12(IDirect3DSurface9 *pSrcRGB, IDirect3DSurface9 *pNV12Dst, uint32_t uWidth, uint32_t uHeight);
};

// NVEncodeAPI entry point
typedef NVENCSTATUS (NVENCAPI *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 
