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

#include "pch.h"

#include<string>
#include "NvEncoderCudaInterop.h"
#include "../common/inc/nvUtils.h"
#include "../common/inc/nvFileIO.h"
#include "../common/inc/helper_string.h"
#include "dynlink_builtin_types.h"

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64) || defined(__aarch64__)
#define PTX_FILE "preproc64_cuda.ptx"
#else
#define PTX_FILE "preproc32_cuda.ptx"
#endif
using namespace std;

#define __cu(a) do { CUresult  ret; if ((ret = (a)) != CUDA_SUCCESS) { fprintf(stderr, "%s has returned CUDA error %d\n", #a, ret); return NV_ENC_ERR_GENERIC;}} while(0)

#define BITSTREAM_BUFFER_SIZE 2*1024*1024

CNvEncoderCudaInterop::CNvEncoderCudaInterop()
{
    m_pNvHWEncoder = new CNvHWEncoder;

    m_cuContext = NULL;
    m_cuModule  = NULL;
    m_cuInterleaveUVFunction = NULL;

    m_uEncodeBufferCount = 0;
    memset(&m_stEncoderInput, 0, sizeof(m_stEncoderInput));
    memset(&m_stEOSOutputBfr, 0, sizeof(m_stEOSOutputBfr));

    memset(&m_stEncodeBuffer, 0, sizeof(m_stEncodeBuffer));
    memset(m_ChromaDevPtr, 0, sizeof(m_ChromaDevPtr));
}

CNvEncoderCudaInterop::~CNvEncoderCudaInterop()
{
    if (m_pNvHWEncoder)
    {
        delete m_pNvHWEncoder;
        m_pNvHWEncoder = NULL;
    }
}

NVENCSTATUS CNvEncoderCudaInterop::InitCuda(unsigned int deviceID, const char *exec_path)
{
    CUresult        cuResult            = CUDA_SUCCESS;
    CUdevice        cuDevice            = 0;
    CUcontext       cuContextCurr;
    int  deviceCount = 0;
    int  SMminor = 0, SMmajor = 0;

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    typedef HMODULE CUDADRIVER;
#else
    typedef void *CUDADRIVER;
#endif
    CUDADRIVER hHandleDriver = 0;

    // CUDA interfaces
    __cu(cuInit(0, __CUDA_API_VERSION, hHandleDriver));

    __cu(cuDeviceGetCount(&deviceCount));
    if (deviceCount == 0)
    {
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    if (deviceID > (unsigned int)deviceCount-1)
    {
        PRINTERR("Invalid Device Id = %d\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    // Now we get the actual device
    __cu(cuDeviceGet(&cuDevice, deviceID));

    __cu(cuDeviceComputeCapability(&SMmajor, &SMminor, deviceID));
    if (((SMmajor << 4) + SMminor) < 0x30)
    {
        PRINTERR("GPU %d does not have NVENC capabilities exiting\n", deviceID);
        return NV_ENC_ERR_NO_ENCODE_DEVICE;
    }

    // Create the CUDA Context and Pop the current one
    __cu(cuCtxCreate(&m_cuContext, 0, cuDevice));

    // in this branch we use compilation with parameters
    const unsigned int jitNumOptions = 3;
    CUjit_option *jitOptions = new CUjit_option[jitNumOptions];
    void **jitOptVals = new void *[jitNumOptions];

    // set up size of compilation log buffer
    jitOptions[0] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
    int jitLogBufferSize = 1024;
    jitOptVals[0] = (void *)(size_t)jitLogBufferSize;

    // set up pointer to the compilation log buffer
    jitOptions[1] = CU_JIT_INFO_LOG_BUFFER;
    char *jitLogBuffer = new char[jitLogBufferSize];
    jitOptVals[1] = jitLogBuffer;

    // set up pointer to set the Maximum # of registers for a particular kernel
    jitOptions[2] = CU_JIT_MAX_REGISTERS;
    int jitRegCount = 32;
    jitOptVals[2] = (void *)(size_t)jitRegCount;

    string ptx_source;
    char *ptx_path = sdkFindFilePath(PTX_FILE, exec_path);
    if (ptx_path == NULL) {
        PRINTERR("Unable to find ptx file path %s\n", PTX_FILE);
        return NV_ENC_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(ptx_path, "rb");
    if (!fp)
    {
        PRINTERR("Unable to read ptx file %s\n", PTX_FILE);
        return NV_ENC_ERR_INVALID_PARAM;
    }
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    char *buf = new char[file_size + 1];
    fseek(fp, 0, SEEK_SET);
    fread(buf, sizeof(char), file_size, fp);
    fclose(fp);
    buf[file_size] = '\0';
    ptx_source = buf;
    delete[] buf;

    cuResult = cuModuleLoadDataEx(&m_cuModule, ptx_source.c_str(), jitNumOptions, jitOptions, (void **)jitOptVals);
    if (cuResult != CUDA_SUCCESS)
    {
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    delete[] jitOptions;
    delete[] jitOptVals;
    delete[] jitLogBuffer;

    __cu(cuModuleGetFunction(&m_cuInterleaveUVFunction, m_cuModule, "InterleaveUV"));

    __cu(cuCtxPopCurrent(&cuContextCurr));
    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderCudaInterop::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);

    CCudaAutoLock cuLock(m_cuContext);

    __cu(cuMemAlloc(&m_ChromaDevPtr[0], uInputWidth*uInputHeight / 4));
    __cu(cuMemAlloc(&m_ChromaDevPtr[1], uInputWidth*uInputHeight / 4));

    __cu(cuMemAllocHost((void **)&m_yuv[0], uInputWidth*uInputHeight));
    __cu(cuMemAllocHost((void **)&m_yuv[1], uInputWidth*uInputHeight / 4));
    __cu(cuMemAllocHost((void **)&m_yuv[2], uInputWidth*uInputHeight / 4));

    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        __cu(cuMemAllocPitch(&m_stEncodeBuffer[i].stInputBfr.pNV12devPtr, (size_t *)&m_stEncodeBuffer[i].stInputBfr.uNV12Stride, uInputWidth, uInputHeight * 3 / 2, 16));

        nvStatus = m_pNvHWEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void*)m_stEncodeBuffer[i].stInputBfr.pNV12devPtr, 
            uInputWidth, uInputHeight, m_stEncodeBuffer[i].stInputBfr.uNV12Stride, &m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
        m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
        m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;

        nvStatus = m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;

        if (m_stEncoderInput.enableAsyncMode)
        {
            nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            if (nvStatus != NV_ENC_SUCCESS)
                return nvStatus;
            m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
        }
        else
            m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
    }

    m_stEOSOutputBfr.bEOSFlag = TRUE;
    if (m_stEncoderInput.enableAsyncMode)
    {
        nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
    }
    else
        m_stEOSOutputBfr.hOutputEvent = NULL;

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderCudaInterop::ReleaseIOBuffers()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    CCudaAutoLock cuLock(m_cuContext);

    for (int i = 0; i < 2; i++)
    {
        if (m_ChromaDevPtr[i])
        {
            cuMemFree(m_ChromaDevPtr[i]);
        }
    }

    for (int i = 0; i < 3; i++)
    {
        if (m_yuv[i])
        {
            cuMemFreeHost(m_yuv[i]);
        }
    }

    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        nvStatus = m_pNvHWEncoder->NvEncUnregisterResource(m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        cuMemFree(m_stEncodeBuffer[i].stInputBfr.pNV12devPtr);

        m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;

        if (m_stEncoderInput.enableAsyncMode)
        {
            m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            nvCloseFile(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
            m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
        }
    }

    if (m_stEOSOutputBfr.hOutputEvent)
    {
        if (m_stEncoderInput.enableAsyncMode)
        {
            m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEOSOutputBfr.hOutputEvent);
            nvCloseFile(m_stEOSOutputBfr.hOutputEvent);
            m_stEOSOutputBfr.hOutputEvent = NULL;
        }
    }

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderCudaInterop::FlushEncoder()
{
    NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
    if(nvStatus != NV_ENC_SUCCESS)
    {
        assert(0);
        return nvStatus;
    }

    EncodeBuffer *pEncodeBuffer = m_EncodeBufferQueue.GetPending();
    while(pEncodeBuffer)
    {
        m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
        // UnMap the input buffer after frame is done
        if (pEncodeBuffer->stInputBfr.hInputSurface)
        {
            nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
            pEncodeBuffer->stInputBfr.hInputSurface = NULL;
        }
        pEncodeBuffer = m_EncodeBufferQueue.GetPending();
    }
#if defined(NV_WINDOWS)
    if (m_stEncoderInput.enableAsyncMode)
    {
        if (WaitForSingleObject(m_stEOSOutputBfr.hOutputEvent, 500) != WAIT_OBJECT_0)
        {
            assert(0);
            nvStatus = NV_ENC_ERR_GENERIC;
        }
    }
#endif
    return nvStatus;
}

NVENCSTATUS CNvEncoderCudaInterop::Deinitialize()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    ReleaseIOBuffers();

    nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
    if (nvStatus != NV_ENC_SUCCESS)
    {
        assert(0);
    }

    __cu(cuCtxDestroy(m_cuContext));

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderCudaInterop::ConvertYUVToNV12(EncodeBuffer *pEncodeBuffer, unsigned char *yuv[3], int width, int height)
{
    CCudaAutoLock cuLock(m_cuContext);
    // copy luma
    CUDA_MEMCPY2D copyParam;
    memset(&copyParam, 0, sizeof(copyParam));
    copyParam.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    copyParam.dstDevice = pEncodeBuffer->stInputBfr.pNV12devPtr;
    copyParam.dstPitch = pEncodeBuffer->stInputBfr.uNV12Stride;
    copyParam.srcMemoryType = CU_MEMORYTYPE_HOST;
    copyParam.srcHost = yuv[0];
    copyParam.srcPitch = width;
    copyParam.WidthInBytes = width;
    copyParam.Height = height;
    __cu(cuMemcpy2D(&copyParam));

    // copy chroma

    __cu(cuMemcpyHtoD(m_ChromaDevPtr[0], yuv[1], width*height / 4));
    __cu(cuMemcpyHtoD(m_ChromaDevPtr[1], yuv[2], width*height / 4));

#define BLOCK_X 32
#define BLOCK_Y 16
    int chromaHeight = height / 2;
    int chromaWidth = width / 2;
    dim3 block(BLOCK_X, BLOCK_Y, 1);
    dim3 grid((chromaWidth + BLOCK_X - 1) / BLOCK_X, (chromaHeight + BLOCK_Y - 1) / BLOCK_Y, 1);
#undef BLOCK_Y
#undef BLOCK_X

    CUdeviceptr dNV12Chroma = (CUdeviceptr)((unsigned char*)pEncodeBuffer->stInputBfr.pNV12devPtr + pEncodeBuffer->stInputBfr.uNV12Stride*height);
    void *args[8] = { &m_ChromaDevPtr[0], &m_ChromaDevPtr[1], &dNV12Chroma, &chromaWidth, &chromaHeight, &chromaWidth, &chromaWidth, &pEncodeBuffer->stInputBfr.uNV12Stride};

    __cu(cuLaunchKernel(m_cuInterleaveUVFunction, grid.x, grid.y, grid.z,
        block.x, block.y, block.z,
        0,
        NULL, args, NULL));
    CUresult cuResult = cuStreamQuery(NULL);
    if (!((cuResult == CUDA_SUCCESS) || (cuResult == CUDA_ERROR_NOT_READY)))
    {
        return NV_ENC_ERR_GENERIC;
    }
    return NV_ENC_SUCCESS;
}

NVENCSTATUS loadframe(uint8_t *yuvInput[3], HANDLE hInputYUVFile, uint32_t frmIdx, uint32_t width, uint32_t height, uint32_t &numBytesRead)
{
    uint64_t fileOffset;
    uint32_t result;
    uint32_t dwInFrameSize = width*height + (width*height)/2;
    fileOffset = (uint64_t)dwInFrameSize * frmIdx;
    result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER)
    {
        return NV_ENC_ERR_INVALID_PARAM;
    }
    nvReadFile(hInputYUVFile, yuvInput[0], width * height, &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[1], width * height / 4, &numBytesRead, NULL);
    nvReadFile(hInputYUVFile, yuvInput[2], width * height / 4, &numBytesRead, NULL);

    return NV_ENC_SUCCESS;
}

void PrintHelp()
{
    printf("Usage : NvEncoderCudaInterop \n"
        "-i <string>                  Specify input yuv420 file\n"
        "-o <string>                  Specify output bitstream file\n"
        "-size <int int>              Specify input resolution <width height>\n"
        "\n### Optional parameters ###\n"
        "-startf <integer>            Specify start index for encoding. Default is 0\n"
        "-endf <integer>              Specify end index for encoding. Default is end of file\n"
        "-codec <integer>             Specify the codec \n"
        "                                 0: H264\n"
        "                                 1: HEVC\n"
        "-preset <string>             Specify the preset for encoder settings\n"
        "                                 hq : nvenc HQ \n"
        "                                 hp : nvenc HP \n"
        "                                 lowLatencyHP : nvenc low latency HP \n"
        "                                 lowLatencyHQ : nvenc low latency HQ \n"
        "                                 lossless : nvenc Lossless HP \n"
        "-fps <integer>               Specify encoding frame rate\n"
        "-goplength <integer>         Specify gop length\n"
        "-numB <integer>              Specify number of B frames\n"
        "-bitrate <integer>           Specify the encoding average bitrate\n"
        "-vbvMaxBitrate <integer>     Specify the vbv max bitrate\n"
        "-vbvSize <integer>           Specify the encoding vbv/hrd buffer size\n"
        "-rcmode <integer>            Specify the rate control mode\n"
        "                                 0:  Constant QP\n"
        "                                 1:  Single pass VBR\n"
        "                                 2:  Single pass CBR\n"
        "                                 4:  Single pass VBR minQP\n"
        "                                 8:  Two pass frame quality\n"
        "                                 16: Two pass frame size cap\n"
        "                                 32: Two pass VBR\n"
        "-qp <integer>                Specify qp for Constant QP mode\n"
        "-i_qfactor <float>           Specify qscale difference between I-frames and P-frames\n"
        "-b_qfactor <float>           Specify qscale difference between P-frames and B-frames\n" 
        "-i_qoffset <float>           Specify qscale offset between I-frames and P-frames\n"
        "-b_qoffset <float>           Specify qscale offset between P-frames and B-frames\n" 
        "-deviceID <integer>          Specify the GPU device on which encoding will take place\n"
        "-help                        Prints Help Information\n\n"
        );
}

int CNvEncoderCudaInterop::EncodeMain(int argc, char *argv[])
{
    HANDLE hInput;
    uint32_t numBytesRead = 0;
    unsigned long long lStart, lEnd, lFreq;
    int numFramesEncoded = 0;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    bool bError = false;
    EncodeBuffer *pEncodeBuffer;
    EncodeConfig encodeConfig;

    memset(&encodeConfig, 0, sizeof(EncodeConfig));

    encodeConfig.endFrameIdx = INT_MAX;
    encodeConfig.bitrate = 5000000;
    encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
    encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
    encodeConfig.deviceType = NV_ENC_CUDA;
    encodeConfig.codec = NV_ENC_H264;
    encodeConfig.fps = 30;
    encodeConfig.qp = 28;
    encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
    encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;  
    encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
    encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET; 
    encodeConfig.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
    encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;

    nvStatus = m_pNvHWEncoder->ParseArguments(&encodeConfig, argc, argv);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        PrintHelp();
        return 1;
    }

    if (!encodeConfig.inputFileName || !encodeConfig.outputFileName || encodeConfig.width == 0 || encodeConfig.height == 0)
    {
        PrintHelp();
        return 1;
    }

    encodeConfig.fOutput = fopen(encodeConfig.outputFileName, "wb");
    if (encodeConfig.fOutput == NULL)
    {
        PRINTERR("Failed to create \"%s\"\n", encodeConfig.outputFileName);
        return 1;
    }

    hInput = nvOpenFile(encodeConfig.inputFileName);
    if (hInput == INVALID_HANDLE_VALUE)
    {
        PRINTERR("Failed to open \"%s\"\n", encodeConfig.inputFileName);
        return 1;
    }

    // initialize Cuda
    nvStatus = InitCuda(encodeConfig.deviceID, argv[0]);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

    nvStatus = m_pNvHWEncoder->Initialize((void*)m_cuContext, NV_ENC_DEVICE_TYPE_CUDA);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);
    
    printf("Encoding input           : \"%s\"\n", encodeConfig.inputFileName);
    printf("         output          : \"%s\"\n", encodeConfig.outputFileName);
    printf("         codec           : \"%s\"\n", encodeConfig.codec == NV_ENC_HEVC ? "HEVC" : "H264");
    printf("         size            : %dx%d\n", encodeConfig.width, encodeConfig.height);
    printf("         bitrate         : %d bits/sec\n", encodeConfig.bitrate);
    printf("         vbvMaxBitrate   : %d bits/sec\n", encodeConfig.vbvMaxBitrate);
    printf("         vbvSize         : %d bits\n", encodeConfig.vbvSize);
    printf("         fps             : %d frames/sec\n", encodeConfig.fps);
    printf("         rcMode          : %s\n", encodeConfig.rcMode == NV_ENC_PARAMS_RC_CONSTQP ? "CONSTQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR ? "VBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_CBR ? "CBR" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_VBR_MINQP ? "VBR MINQP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_QUALITY ? "TWO_PASS_QUALITY" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP ? "TWO_PASS_FRAMESIZE_CAP" :
                                              encodeConfig.rcMode == NV_ENC_PARAMS_RC_2_PASS_VBR ? "TWO_PASS_VBR" : "UNKNOWN");
    if (encodeConfig.gopLength == NVENC_INFINITE_GOPLENGTH)
        printf("         goplength       : INFINITE GOP \n");
    else
        printf("         goplength       : %d \n", encodeConfig.gopLength);
    printf("         B frames        : %d \n", encodeConfig.numB);
    printf("         QP              : %d \n", encodeConfig.qp);
    printf("         preset          : %s\n", (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HQ_GUID) ? "LOW_LATENCY_HQ" :
        (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HP_GUID) ? "LOW_LATENCY_HP" :
        (encodeConfig.presetGUID == NV_ENC_PRESET_HQ_GUID) ? "HQ_PRESET" :
        (encodeConfig.presetGUID == NV_ENC_PRESET_HP_GUID) ? "HP_PRESET" :
        (encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID) ? "LOSSLESS_HP" :
        (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID) ? "LOW_LATENCY_DEFAULT" : "DEFAULT");
    printf("\n");

    nvStatus = m_pNvHWEncoder->CreateEncoder(&encodeConfig);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    m_uEncodeBufferCount = encodeConfig.numB + 4;
    m_stEncoderInput.enableAsyncMode = encodeConfig.enableAsyncMode;

    nvStatus = AllocateIOBuffers(encodeConfig.width, encodeConfig.height);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    NvQueryPerformanceCounter(&lStart);

    for (int frm = encodeConfig.startFrameIdx; frm <= encodeConfig.endFrameIdx; frm++)
    {
       numBytesRead = 0;
       loadframe(m_yuv, hInput, frm, encodeConfig.width, encodeConfig.height, numBytesRead);
        if (numBytesRead == 0)
            break;

        EncodeFrameConfig stEncodeFrame;
        memset(&stEncodeFrame, 0, sizeof(stEncodeFrame));
        stEncodeFrame.yuv[0] = m_yuv[0];
        stEncodeFrame.yuv[1] = m_yuv[1];
        stEncodeFrame.yuv[2] = m_yuv[2];

        stEncodeFrame.stride[0] = encodeConfig.width;
        stEncodeFrame.stride[1] = encodeConfig.width / 2;
        stEncodeFrame.stride[2] = encodeConfig.width / 2;
        stEncodeFrame.width = encodeConfig.width;
        stEncodeFrame.height = encodeConfig.height;

        pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
        if(!pEncodeBuffer)
        {
            pEncodeBuffer = m_EncodeBufferQueue.GetPending();
            m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
            // UnMap the input buffer after frame done
            if (pEncodeBuffer->stInputBfr.hInputSurface)
            {
                nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
                pEncodeBuffer->stInputBfr.hInputSurface = NULL;
            }
            pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
        }

        ConvertYUVToNV12(pEncodeBuffer, stEncodeFrame.yuv, encodeConfig.width, encodeConfig.height);

        nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            PRINTERR("Failed to Map input buffer %p\n", pEncodeBuffer->stInputBfr.hInputSurface);
            return nvStatus;
        }

        m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, encodeConfig.width, encodeConfig.height);
        numFramesEncoded++;
    }

    FlushEncoder();

    if (numFramesEncoded > 0)
    {
        NvQueryPerformanceCounter(&lEnd);
        NvQueryPerformanceFrequency(&lFreq);
        double elapsedTime = (double)(lEnd - lStart);
        printf("Encoded %d frames in %6.2fms\n", numFramesEncoded, (elapsedTime*1000.0)/lFreq);
        printf("Avergage Encode Time : %6.2fms\n", ((elapsedTime*1000.0)/numFramesEncoded)/lFreq);
    }

    if (encodeConfig.fOutput)
    {
        fclose(encodeConfig.fOutput);
    }

    if (hInput)
    {
        nvCloseFile(hInput);
    }

    Deinitialize();
    
    return bError ? 1 : 0;
}

int main(int argc, char **argv)
{
    CNvEncoderCudaInterop nvEncoder;
    return nvEncoder.EncodeMain(argc, argv);
}
