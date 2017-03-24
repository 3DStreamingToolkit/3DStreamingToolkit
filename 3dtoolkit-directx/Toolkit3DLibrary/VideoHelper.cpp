#pragma once

#include "pch.h"

#include "VideoHelper.h"
#include "nvFileIO.h"
#include "nvUtils.h"

using namespace Toolkit3DLibrary;

// Constructor for VideoHelper.
VideoHelper::VideoHelper(ID3D11Device* device, ID3D11DeviceContext* context) :
	m_d3dDevice(device),
	m_d3dContext(context)
{
	m_pNvHWEncoder = new CNvHWEncoder();
}

// Destructor for VideoHelper.
VideoHelper::~VideoHelper()
{
	Deinitialize();

	if (m_pNvHWEncoder)
	{
		delete m_pNvHWEncoder;
		m_pNvHWEncoder = NULL;
	}

	SAFE_RELEASE(m_stagingFrameBuffer);
}

// Initializes the swap chain and IO buffers.
void VideoHelper::Initialize(IDXGISwapChain* swapChain, char* outputFile)
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChain->GetDesc(&swapChainDesc);
	m_swapChain = swapChain;

	memset(&m_encodeConfig, 0, sizeof(EncodeConfig));
	m_encodeConfig.outputFileName = outputFile;
	m_encodeConfig.width = swapChainDesc.BufferDesc.Width;
	m_encodeConfig.height = swapChainDesc.BufferDesc.Height;
	m_encodeConfig.endFrameIdx = INT_MAX;
	m_encodeConfig.bitrate = 5000000;
	m_encodeConfig.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
	m_encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
	m_encodeConfig.deviceType = 0;
	m_encodeConfig.codec = NV_ENC_H264;
	m_encodeConfig.fps = 60;
	m_encodeConfig.qp = 28;
	m_encodeConfig.i_quant_factor = DEFAULT_I_QFACTOR;
	m_encodeConfig.b_quant_factor = DEFAULT_B_QFACTOR;
	m_encodeConfig.i_quant_offset = DEFAULT_I_QOFFSET;
	m_encodeConfig.b_quant_offset = DEFAULT_B_QOFFSET;
	m_encodeConfig.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
	m_encodeConfig.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	m_encodeConfig.fOutput = fopen(m_encodeConfig.outputFileName, "wb");
	if (m_encodeConfig.fOutput == NULL)
	{
		PRINTERR("Failed to create \"%s\"\n", m_encodeConfig.outputFileName);
	}

	nvStatus = m_pNvHWEncoder->Initialize((void*)m_d3dDevice, NV_ENC_DEVICE_TYPE_DIRECTX);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}

	m_encodeConfig.presetGUID = m_pNvHWEncoder->GetPresetGUID(m_encodeConfig.encoderPreset, m_encodeConfig.codec);

	// Prints config info to console.
	PrintConfig(m_encodeConfig);

	// Creates the encoder.
	nvStatus = m_pNvHWEncoder->CreateEncoder(&m_encodeConfig);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}

	m_uEncodeBufferCount = m_encodeConfig.numB + 4;

	nvStatus = AllocateIOBuffers(m_encodeConfig.width, m_encodeConfig.height, swapChainDesc);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return;
	}

	// Initializes the input buffer, backed by ID3D11Texture2D*.
	m_stagingFrameBufferDesc = { 0 };
	m_stagingFrameBufferDesc.ArraySize = 1;
	m_stagingFrameBufferDesc.Format = swapChainDesc.BufferDesc.Format;
	m_stagingFrameBufferDesc.Width = swapChainDesc.BufferDesc.Width;
	m_stagingFrameBufferDesc.Height = swapChainDesc.BufferDesc.Height;
	m_stagingFrameBufferDesc.MipLevels = 1;
	m_stagingFrameBufferDesc.SampleDesc.Count = 1;
	m_stagingFrameBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	m_stagingFrameBufferDesc.Usage = D3D11_USAGE_STAGING;
	m_d3dDevice->CreateTexture2D(&m_stagingFrameBufferDesc, nullptr, &m_stagingFrameBuffer);
}

// Cleanup resources.
NVENCSTATUS VideoHelper::Deinitialize()
{
	FlushEncoder();
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	ReleaseIOBuffers();
	nvStatus = m_pNvHWEncoder->NvEncDestroyEncoder();
	return nvStatus;
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture()
{
	// Try to process the pending input buffers.
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	EncodeBuffer* pEncodeBuffer = m_EncodeBufferQueue.GetAvailable();
	if (!pEncodeBuffer)
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

	// Gets the frame buffer from the swap chain.
	ID3D11Texture2D* frameBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer));

	// Copies the frame buffer to the encode input buffer.
	m_d3dContext->CopyResource(pEncodeBuffer->stInputBfr.pARGBSurface, frameBuffer);
	frameBuffer->Release();
	nvStatus = m_pNvHWEncoder->NvEncMapInputResource(pEncodeBuffer->stInputBfr.nvRegisteredResource, &pEncodeBuffer->stInputBfr.hInputSurface);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		PRINTERR("Failed to Map input buffer %p\n", pEncodeBuffer->stInputBfr.hInputSurface);
		return;
	}

	// Encoding.
	if (SUCCEEDED(hr))
	{
		nvStatus = m_pNvHWEncoder->NvEncEncodeFrame(pEncodeBuffer, NULL, m_encodeConfig.width, m_encodeConfig.height);
		if (nvStatus != NV_ENC_SUCCESS  && nvStatus != NV_ENC_ERR_NEED_MORE_INPUT)
		{
			return;
		}
	}
}

// Captures frame buffer from the swap chain.
void VideoHelper::Capture(void** buffer, int* size, int* width, int* height)
{
	// Gets the frame buffer from the swap chain.
	ID3D11Texture2D* frameBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer(0,
		__uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&frameBuffer));

	if (FAILED(hr))
	{
		return;
	}

	// Copies the frame buffer to the encode input buffer.
	m_d3dContext->CopyResource(m_stagingFrameBuffer, frameBuffer);
	frameBuffer->Release();

	// Accesses frame buffer
	D3D11_MAPPED_SUBRESOURCE mapped;
	hr = m_d3dContext->Map(m_stagingFrameBuffer, 0, D3D11_MAP_READ, 0, &mapped);

	if (SUCCEEDED(hr))
	{
		m_d3dContext->Unmap(m_stagingFrameBuffer, 0);
		int bufferWidth = m_stagingFrameBufferDesc.Width;
		int half_width = bufferWidth >> 1;
		int bufferHeight = m_stagingFrameBufferDesc.Height;
		size_t size_y = static_cast<size_t>(bufferWidth) * bufferHeight;
		size_t size_uv = static_cast<size_t>(half_width) * ((bufferHeight + 1) / 2);
		*buffer = (char*)mapped.pData;
		*size = size_y + size_uv + size_uv;
		*width = bufferWidth;
		*height = bufferHeight;
	}
}

void VideoHelper::CaptureCompressedFrame(void** buffer, int* size)
{
	NV_ENC_LOCK_BITSTREAM bitStream = m_pNvHWEncoder->GetLockBitStream();
	*buffer = bitStream.bitstreamBufferPtr;
	*size = bitStream.bitstreamSizeInBytes;
}

NVENCSTATUS VideoHelper::AllocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, DXGI_SWAP_CHAIN_DESC swapChainDesc)
{
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	ID3D11Texture2D* pVPSurfaces[16];

	// Initializes the encode buffer queue.
	m_EncodeBufferQueue.Initialize(m_stEncodeBuffer, m_uEncodeBufferCount);

	// Finds the suitable format for buffer.
	DXGI_FORMAT format = swapChainDesc.BufferDesc.Format;
	if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
	{
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
	}

	for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
	{
		// Initializes the input buffer, backed by ID3D11Texture2D*.
		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.ArraySize = 1;
		desc.Format = format;
		desc.Width = uInputWidth;
		desc.Height = uInputHeight;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		m_d3dDevice->CreateTexture2D(&desc, nullptr, &pVPSurfaces[i]);

		// Registers the input buffer with NvEnc.
		nvStatus = m_pNvHWEncoder->NvEncRegisterResource(
			NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX,
			(void*)pVPSurfaces[i],
			uInputWidth,
			uInputHeight,
			m_stEncodeBuffer[i].stInputBfr.uARGBStride,
			&m_stEncodeBuffer[i].stInputBfr.nvRegisteredResource);
		
		// Fails to register the input buffer with NvEnc.
		if (nvStatus != NV_ENC_SUCCESS)
		{
			return nvStatus;
		}

		// Maps the buffer format to the relevant NvEnc encoder format
		switch (format)
		{
			case DXGI_FORMAT_B8G8R8A8_UNORM:
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
				break;

			case DXGI_FORMAT_R10G10B10A2_UNORM:
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
				break;

			default:
				m_stEncodeBuffer[i].stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_ABGR;
				break;
		}

		m_stEncodeBuffer[i].stInputBfr.dwWidth = uInputWidth;
		m_stEncodeBuffer[i].stInputBfr.dwHeight = uInputHeight;
		m_stEncodeBuffer[i].stInputBfr.pARGBSurface = pVPSurfaces[i];

		// Initializes the output buffer.
		nvStatus = m_pNvHWEncoder->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &m_stEncodeBuffer[i].stOutputBfr.hBitstreamBuffer);
		if (nvStatus != NV_ENC_SUCCESS)
		{
			return nvStatus;
		}

		m_stEncodeBuffer[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;

		// Registers for the output event.
		nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEncodeBuffer[i].stOutputBfr.hOutputEvent);
		if (nvStatus != NV_ENC_SUCCESS)
		{
			return nvStatus;
		}

		m_stEncodeBuffer[i].stOutputBfr.bWaitOnEvent = true;
	}

	m_stEOSOutputBfr.bEOSFlag = TRUE;

	// Registers for the output event.
	nvStatus = m_pNvHWEncoder->NvEncRegisterAsyncEvent(&m_stEOSOutputBfr.hOutputEvent);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		return nvStatus;
	}

	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoHelper::ReleaseIOBuffers()
{
	for (uint32_t i = 0; i < m_uEncodeBufferCount; i++)
	{
		SAFE_RELEASE(m_stEncodeBuffer[i].stInputBfr.pARGBSurface);

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
		m_stEOSOutputBfr.hOutputEvent = NULL;
	}

	return NV_ENC_SUCCESS;
}

NVENCSTATUS VideoHelper::FlushEncoder()
{
	NVENCSTATUS nvStatus = m_pNvHWEncoder->NvEncFlushEncoderQueue(m_stEOSOutputBfr.hOutputEvent);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		assert(0);
		return nvStatus;
	}

	EncodeBuffer *pEncodeBuffer = m_EncodeBufferQueue.GetPending();
	while (pEncodeBuffer)
	{
		m_pNvHWEncoder->ProcessOutput(pEncodeBuffer);
		pEncodeBuffer = m_EncodeBufferQueue.GetPending();

		// UnMap the input buffer after frame is done.
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

void VideoHelper::PrintConfig(EncodeConfig encodeConfig)
{
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
	{
		printf("         goplength       : INFINITE GOP \n");
	}
	else
	{
		printf("         goplength       : %d \n", encodeConfig.gopLength);
	}

	printf("         B frames        : %d \n", encodeConfig.numB);
	printf("         QP              : %d \n", encodeConfig.qp);
	printf("         preset          : %s\n", (encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HQ_GUID) ? "LOW_LATENCY_HQ" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_HP_GUID) ? "LOW_LATENCY_HP" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_HQ_GUID) ? "HQ_PRESET" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_HP_GUID) ? "HP_PRESET" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOSSLESS_HP_GUID) ? "LOSSLESS_HP" :
		(encodeConfig.presetGUID == NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID) ? "LOW_LATENCY_DEFAULT" : "DEFAULT");
	
	printf("\n");
}