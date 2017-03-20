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
#include <initguid.h>
#include "NvEncoderD3DInterop.h"
#include "../common/inc/nvFileIO.h"
#include "../common/inc/nvUtils.h"

const D3DFORMAT D3DFMT_NV12 = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');

#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024

bool LoadBMP(char * fileName, unsigned char ** pp, DWORD * pw, DWORD * ph, DWORD * pbits)
{
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

    FILE * fin;

    if (fin = fopen(fileName, "rb"))
    {
        if ((fread(((BYTE *)(&bmfh)), sizeof(BITMAPFILEHEADER), 1, fin) != 1) || (fread(((BYTE *)(&bmih)), sizeof(BITMAPINFOHEADER), 1, fin) != 1))
        {
            fclose(fin);
            return false;
        }

        *pw = bmih.biWidth;
        *ph = bmih.biHeight;
        *pbits = bmih.biBitCount;
        if (*pbits != 24 && *pbits != 32)
        {
            fclose(fin);
            return false;
        }

        bmih.biSizeImage = *pbits * bmih.biWidth * bmih.biHeight / 8;

        *pp = (unsigned char *)malloc(bmih.biSizeImage);

        if (!*pp)
        {
            fclose(fin);
            return false;
        }

        if (fread(*pp, bmih.biSizeImage, 1, fin) != 1)
        {
            free(*pp);
            fclose(fin);
            return false;
        }

        fclose(fin);
        return true;
    }

    return false;
}

bool LoadBMPWithReorderLines(char * fileName, uint8_t *pRGB, DWORD dwStride)
{
    unsigned char * lp;
    DWORD w, h, bits;

    if (!LoadBMP(fileName, &lp, &w, &h, &bits))
        return false;

    DWORD offset = bits / 8;
    for (DWORD j = 0; j<h; j++)
    {
        unsigned char * p3 = lp + (h - 1 - j)*(w * offset);
        unsigned char * p4 = pRGB + j*dwStride;
        for (DWORD i = 0; i<w; i++)
        {
            p4[i * 4 + 0] = p3[i * offset + 0];
            p4[i * 4 + 1] = p3[i * offset + 1];
            p4[i * 4 + 2] = p3[i * offset + 2];
        }
    }

    free(lp);

    return true;
}

CNvEncoderD3DInterop::CNvEncoderD3DInterop()
{
    m_pNvHWEncoder = new CNvHWEncoder;

    m_pD3DEx = NULL;
    m_pD3D9Device = NULL;
    m_pDXVA2VideoProcessServices = NULL;
    m_pDXVA2VideoProcessor = NULL;

    m_uEncodeBufferCount = 0;

    memset(&m_stEOSOutputBfr, 0, sizeof(m_stEOSOutputBfr));
    memset(&m_stEncodeBuffer, 0, sizeof(m_stEncodeBuffer));

    memset(&m_Brightness, 0, sizeof(m_Brightness));
    memset(&m_Contrast, 0, sizeof(m_Contrast));
    memset(&m_Hue, 0, sizeof(m_Hue));
    memset(&m_Saturation, 0, sizeof(m_Saturation));
}

CNvEncoderD3DInterop::~CNvEncoderD3DInterop()
{
    if (m_pNvHWEncoder)
    {
        delete m_pNvHWEncoder;
        m_pNvHWEncoder = NULL;
    }
}

NVENCSTATUS CNvEncoderD3DInterop::InitD3D9(unsigned int deviceID)
{
    HRESULT hr = S_OK;
    D3DPRESENT_PARAMETERS d3dpp = {0};
    D3DADAPTER_IDENTIFIER9 adapterId = {0};
    unsigned int iAdapter = NULL;

    Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3DEx);

    if (deviceID >= m_pD3DEx->GetAdapterCount())
    {
        PRINTERR("nvEncoder (-deviceID=%d) is not a valid GPU device. Headless video devices will not be detected.  <<\n\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    hr = m_pD3DEx->GetAdapterIdentifier(deviceID, 0, &adapterId);
    if (hr != S_OK)
    {
        PRINTERR("nvEncoder (-deviceID=%d) is not a valid GPU device. <<\n\n", deviceID);
        return NV_ENC_ERR_INVALID_ENCODERDEVICE;
    }

    // Create the Direct3D9 device and the swap chain. In this example, the swap
    // chain is the same size as the current display mode. The format is RGB-32.
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = true;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
    DWORD dwBehaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING;
    hr = m_pD3DEx->CreateDevice(deviceID,
        D3DDEVTYPE_HAL,
        NULL,
        dwBehaviorFlags,
        &d3dpp,
        &m_pD3D9Device);

    hr = DXVA2CreateVideoService(m_pD3D9Device, IID_PPV_ARGS(&m_pDXVA2VideoProcessServices));

    DXVA2_VideoDesc vd = {0};
    unsigned int uGuidCount = 0;
    GUID *pGuids = NULL;
    bool bVPGuidAvailable = false;
    hr = m_pDXVA2VideoProcessServices->GetVideoProcessorDeviceGuids(&vd, &uGuidCount, &pGuids);
    for (unsigned int i = 0; i < uGuidCount; i++)
    {
        if (pGuids[i] == DXVA2_VideoProcProgressiveDevice)
        {
            bVPGuidAvailable = true;
            break;
        }
    }
    CoTaskMemFree(pGuids);
    if (!bVPGuidAvailable)
    {
        return NV_ENC_ERR_OUT_OF_MEMORY;
    }

    if (bVPGuidAvailable)
    {
        hr = m_pDXVA2VideoProcessServices->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice, &vd, D3DFMT_NV12, 0, &m_pDXVA2VideoProcessor);
        hr = m_pDXVA2VideoProcessServices->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &vd, D3DFMT_NV12, DXVA2_ProcAmp_Brightness, &m_Brightness);
        hr = m_pDXVA2VideoProcessServices->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &vd, D3DFMT_NV12, DXVA2_ProcAmp_Contrast, &m_Contrast);
        hr = m_pDXVA2VideoProcessServices->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &vd, D3DFMT_NV12, DXVA2_ProcAmp_Hue, &m_Hue);
        hr = m_pDXVA2VideoProcessServices->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &vd, D3DFMT_NV12, DXVA2_ProcAmp_Saturation, &m_Saturation);
    }

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderD3DInterop::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    IDirect3DSurface9 *pVPSurfaces[16];

    m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);

    HRESULT hr = m_pDXVA2VideoProcessServices->CreateSurface(uInputWidth, uInputHeight, m_uEncodeBufferCount - 1,
        D3DFMT_NV12, D3DPOOL_DEFAULT, 0, DXVA2_VideoProcessorRenderTarget, &pVPSurfaces[0], NULL);

    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        nvStatus = m_pNvHWEncoder->NvEncRegisterResource(NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX, (void*)pVPSurfaces[i],
            uInputWidth, uInputHeight, m_stEncodeBuffer[i].stInputBfr.uNV12Stride, &m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;

        m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
        m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
        m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;
        m_stEncodeBuffer[i].stInputBfr.pNV12Surface = pVPSurfaces[i];

        nvStatus = m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;

        nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS)
            return nvStatus;
        m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
    }

    m_stEOSOutputBfr.bEOSFlag = TRUE;
    nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderD3DInterop::ReleaseIOBuffers()
{
    for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
    {
        if (m_stEncodeBuffer[i].stInputBfr.pNV12Surface)
        {
            m_stEncodeBuffer[i].stInputBfr.pNV12Surface->Release();
        }

        m_pNvHWEncoder->NvEncDestroyBitstreamBuffer(m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
        m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer = NULL;

        m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        CloseHandle(m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
        m_stEncodeBuffer[i].stOutputBfr.hOutputEvent = NULL;
    }

    if (m_stEOSOutputBfr.hOutputEvent)
    {
        m_pNvHWEncoder->NvEncUnregisterAsyncEvent(m_stEOSOutputBfr.hOutputEvent);
        CloseHandle(m_stEOSOutputBfr.hOutputEvent);
        m_stEOSOutputBfr.hOutputEvent  = NULL;
    }

    return NV_ENC_SUCCESS;
}

NVENCSTATUS CNvEncoderD3DInterop::FlushEncoder()
{
    NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        assert(0);
        return nvStatus;
    }

    EncodeBuffer *pEncodeBuffer = m_EncodeBufferQueue.GetPending();
    while(pEncodeBuffer)
    {
        m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
        pEncodeBuffer = m_EncodeBufferQueue.GetPending();
        // UnMap the input buffer after frame is done
        if (pEncodeBuffer && pEncodeBuffer->stInputBfr.hInputSurface)
        {
            nvStatus = m_pNvHWEncoder->NvEncUnmapInputResource(pEncodeBuffer->stInputBfr.hInputSurface);
            pEncodeBuffer->stInputBfr.hInputSurface = NULL;
        }
    }

    if (WaitForSingleObject(m_stEOSOutputBfr.hOutputEvent, 500) != WAIT_OBJECT_0)
    {
        assert(0);
        nvStatus = NV_ENC_ERR_GENERIC;
    }

    return nvStatus;
}

NVENCSTATUS CNvEncoderD3DInterop::Deinitialize()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

    ReleaseIOBuffers();
    nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();

    if (m_pDXVA2VideoProcessor)
    {
        m_pDXVA2VideoProcessor->Release();
        m_pDXVA2VideoProcessor = NULL;
    }

    if (m_pDXVA2VideoProcessServices)
    {
        m_pDXVA2VideoProcessServices->Release();
        m_pDXVA2VideoProcessServices = NULL;
    }

    if (m_pD3D9Device)
    {
        m_pD3D9Device->Release();
        m_pD3D9Device = NULL;
    }

    if (m_pD3DEx)
    {
        m_pD3DEx->Release();
        m_pD3DEx = NULL;
    }

    return nvStatus;
}

NVENCSTATUS CNvEncoderD3DInterop::ConvertRGBToNV12(IDirect3DSurface9 *pSrcRGB, IDirect3DSurface9 *pNV12Dst, uint32_t uWidth, uint32_t uHeight)
{
    DXVA2_VideoProcessBltParams vpblt;
    DXVA2_VideoSample vs;

    RECT srcRect = { 0, 0, uWidth, uHeight };
    RECT dstRect = { 0, 0, uWidth, uHeight };
    // Input
    memset(&vs, 0, sizeof(vs));
    vs.PlanarAlpha.ll = 0x10000;
    vs.SrcSurface = pSrcRGB;
    vs.SrcRect = srcRect;
    vs.DstRect = dstRect;
    vs.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    vs.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    vs.SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
    vs.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;

    // Output
    memset(&vpblt, 0, sizeof(vpblt));
    vpblt.TargetRect = dstRect;
    vpblt.DestFormat = vs.SampleFormat;
    vpblt.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    vpblt.Alpha.ll = 0x10000;
    vpblt.TargetFrame = vs.Start;
    vpblt.ProcAmpValues.Brightness = m_Brightness.DefaultValue;
    vpblt.ProcAmpValues.Contrast = m_Contrast.DefaultValue;
    vpblt.ProcAmpValues.Hue = m_Hue.DefaultValue;
    vpblt.ProcAmpValues.Saturation = m_Saturation.DefaultValue;
    vpblt.BackgroundColor.Y = 0x1000;
    vpblt.BackgroundColor.Cb = 0x8000;
    vpblt.BackgroundColor.Cr = 0x8000;
    vpblt.BackgroundColor.Alpha = 0xffff;
    HRESULT hr = m_pDXVA2VideoProcessor->VideoProcessBlt(pNV12Dst, &vpblt, &vs, 1, NULL);
    return NV_ENC_SUCCESS;
}

void PrintHelp()
{
    printf("Usage : NvEncoderD3DInterop \n"
        "-bmpfilePath <string>        Specify input rgb bmp file path\n"
        "-o <string>                  Specify output bitstream file\n"
        "-size <int int>              Specify input resolution <width height>\n"
        "\n### Optional parameters ###\n"
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

int CNvEncoderD3DInterop::EncodeMain(int argc, char *argv[])
{
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
    encodeConfig.deviceType = 0;
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

    if (!encodeConfig.inputFilePath || !encodeConfig.outputFileName || encodeConfig.width == 0 || encodeConfig.height == 0)
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

    // initialize D3D
    nvStatus = InitD3D9(encodeConfig.deviceID);
    if (nvStatus != NV_ENC_SUCCESS)
        return nvStatus;

    nvStatus = m_pNvHWEncoder->Initialize((void*)m_pD3D9Device, NV_ENC_DEVICE_TYPE_DIRECTX);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(encodeConfig.encoderPreset, encodeConfig.codec);

    printf("Encoding BmpFilePath     : \"%s\"\n", encodeConfig.inputFilePath);
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

    nvStatus = AllocateIOBuffers(encodeConfig.width, encodeConfig.height);
    if (nvStatus != NV_ENC_SUCCESS)
        return 1;

    HRESULT hr;
    IDirect3DTexture9      *pRGBTextureSys = NULL;
    IDirect3DTexture9      *pRGBTextureVid = NULL;
    unsigned char *pTexSysBuf = (unsigned char*)malloc(encodeConfig.width*encodeConfig.height * 4);
    hr = m_pD3D9Device->CreateTexture(encodeConfig.width, encodeConfig.height, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &pRGBTextureSys, (HANDLE *)&pTexSysBuf);
    hr = m_pD3D9Device->CreateTexture(encodeConfig.width, encodeConfig.height, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pRGBTextureVid, NULL);
    NvQueryPerformanceCounter(&lStart);


    char pFullName[256];
    HANDLE hFile;
    WIN32_FIND_DATAA FindFileData;
    sprintf(pFullName, "%s\\*.bmp", encodeConfig.inputFilePath);
    hFile = FindFirstFileA(pFullName, &FindFileData);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        PRINTERR("Unable to load bmp file from directory %s\n", encodeConfig.inputFilePath);
        return 1;
    }

    do
    {
        if (FILE_ATTRIBUTE_DIRECTORY & FindFileData.dwFileAttributes)
            continue;
        D3DLOCKED_RECT Rect;
        HRESULT hr = pRGBTextureSys->LockRect(0, &Rect, NULL, D3DLOCK_DISCARD);
        sprintf(pFullName, "%s\\%s", encodeConfig.inputFilePath, FindFileData.cFileName);
        if (!LoadBMPWithReorderLines(pFullName, (unsigned char *)pTexSysBuf, encodeConfig.width * 4))
        {
            PRINTERR("Unable to load the bmp file %s\n", pFullName);
            pRGBTextureSys->UnlockRect(0);
            break;
        }

        pRGBTextureSys->UnlockRect(0);
        m_pD3D9Device->UpdateTexture(pRGBTextureSys, pRGBTextureVid);

        EncodeFrameConfig stEncodeFrame;
        memset(&stEncodeFrame, 0, sizeof(stEncodeFrame));
        stEncodeFrame.pRGBTexture = pRGBTextureVid;
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

        IDirect3DSurface9 *pSrcRGB = NULL;
        hr = stEncodeFrame.pRGBTexture->GetSurfaceLevel(0, &pSrcRGB);
        ConvertRGBToNV12(pSrcRGB, pEncodeBuffer->stInputBfr.pNV12Surface, encodeConfig.width, encodeConfig.height);
        pSrcRGB->Release();

        nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            PRINTERR("Failed to Map input buffer 0x%08x\n", pEncodeBuffer->stInputBfr.hInputSurface);
            bError = true;
            goto exit;
        }

        nvStatus = m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, encodeConfig.width, encodeConfig.height);
        if (nvStatus != NV_ENC_SUCCESS  && nvStatus != NV_ENC_ERR_NEED_MORE_INPUT)
        {
            bError = true;
            goto exit;
        }
        numFramesEncoded++;
    } while (FindNextFileA(hFile, &FindFileData));

    if (numFramesEncoded > 0)
    {
        FlushEncoder();
        if (nvStatus != NV_ENC_SUCCESS)
        {
            bError = true;
            goto exit;
        }
        NvQueryPerformanceCounter(&lEnd);
        NvQueryPerformanceFrequency(&lFreq);
        double elapsedTime = (double)(lEnd - lStart);
        printf("Encoded %d frames in %6.2fms\n", numFramesEncoded, (elapsedTime*1000.0) / lFreq);
        printf("Avergage Encode Time : % 6.2fms\n", ((elapsedTime*1000.0) / numFramesEncoded) / lFreq);
    }

exit:

    if (pRGBTextureSys)
    {
        pRGBTextureSys->Release();
        pRGBTextureSys = NULL;
    }

    if (pTexSysBuf)
    {
        free(pTexSysBuf);
        pTexSysBuf = NULL;
    }

    if (pRGBTextureVid)
    {
        pRGBTextureVid->Release();
        pRGBTextureVid = NULL;

    }

    if (encodeConfig.fOutput)
    {
        fclose(encodeConfig.fOutput);
    }

    Deinitialize();
    
    return bError ? 1 : 0;
}

int main(int argc, char **argv)
{
    CNvEncoderD3DInterop nvEncoder;
    return nvEncoder.EncodeMain(argc, argv);
}
