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


#include "../common/inc/NvHWEncoder.h"

#define MAX_ENCODE_QUEUE 32

#define SET_VER(configStruct, type) {configStruct.version = type##_VER;}

template<class T>
class CNvQueue {
    T** m_pBuffer;
    unsigned int m_uSize;
    unsigned int m_uPendingCount;
    unsigned int m_uAvailableIdx;
    unsigned int m_uPendingndex;
public:
    CNvQueue(): m_pBuffer(NULL), m_uSize(0), m_uPendingCount(0), m_uAvailableIdx(0),
                m_uPendingndex(0)
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

class CCudaAutoLock
{
private:
    CUcontext m_pCtx;
public:
    CCudaAutoLock(CUcontext pCtx) :m_pCtx(pCtx) { cuCtxPushCurrent(m_pCtx); };
    ~CCudaAutoLock()  { CUcontext cuLast = NULL; cuCtxPopCurrent(&cuLast); };
};

typedef enum
{
    NV_ENC_DX9 = 0,
    NV_ENC_DX11 = 1,
    NV_ENC_CUDA = 2,
    NV_ENC_DX10 = 3,
} NvEncodeDeviceType;

typedef struct _EncodeFrameConfig
{
    uint8_t  *yuv[3];
    uint32_t stride[3];
    uint32_t width;
    uint32_t height;
}EncodeFrameConfig;

class CNvEncoderCudaInterop
{
public:
    CNvEncoderCudaInterop();
    virtual ~CNvEncoderCudaInterop();

protected:
    CNvHWEncoder                                        *m_pNvHWEncoder;
    uint32_t                                             m_uEncodeBufferCount;
    CUcontext                                            m_cuContext;
    CUmodule                                             m_cuModule;
    CUfunction                                           m_cuInterleaveUVFunction;
    CUdeviceptr                                          m_ChromaDevPtr[2];
    EncodeConfig                                         m_stEncoderInput;
    EncodeBuffer                                         m_stEncodeBuffer[MAX_ENCODE_QUEUE];
    CNvQueue<EncodeBuffer>                               m_EncodeBufferQueue;
    EncodeOutputBuffer                                   m_stEOSOutputBfr; 
    uint8_t                                             *m_yuv[3];

protected:
    NVENCSTATUS                                          Deinitialize();
    NVENCSTATUS                                          InitCuda(uint32_t deviceID, const char *exec_path);
    NVENCSTATUS                                          AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight);
    NVENCSTATUS                                          ReleaseIOBuffers();
    NVENCSTATUS                                          FlushEncoder();
    NVENCSTATUS                                          ConvertYUVToNV12(EncodeBuffer *pEncodeBuffer, unsigned char *yuv[3], int width, int height);

public:
    int                                                  EncodeMain(int argc, char **argv);
};

// NVEncodeAPI entry point
typedef NVENCSTATUS (NVENCAPI *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 
